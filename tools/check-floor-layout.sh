#!/usr/bin/env bash
# Host-only oracle: compile the v4.0.2 builders and room geometry unchanged.
set -euo pipefail
cd "$(dirname "$0")/.."
upstream=${1:-../spd-upstream}
ref=57a4e06a4caf162446d1c28caa7983f0493fecf0
scratch=$(mktemp -d)
trap 'find "$scratch" -depth -delete' EXIT
game=com.shatteredpixel.shatteredpixeldungeon
dir=${game//.//}
utils=com/watabou/utils
mkdir -p "$scratch/java/$dir/levels" "$scratch/java/$utils" "$scratch/java/com/watabou/noosa"
for file in GameMath Graph Point PointF Random Rect; do
  git -C "$upstream" show "$ref:SPD-classes/src/main/java/$utils/$file.java" > "$scratch/java/$utils/$file.java"
done
for file in Patch features/Maze rooms/Room builders/{Builder,RegularBuilder,LoopBuilder,FigureEightBuilder}; do
  mkdir -p "$scratch/java/$dir/levels/$(dirname "$file")"
  git -C "$upstream" show "$ref:core/src/main/java/$dir/levels/$file.java" > "$scratch/java/$dir/levels/$file.java"
done

# Only content/rendering types are stubbed. The oracle and the native builder
# receive identical explicit room descriptors, not full upstream floor inputs.
stub() {
  local package=$1 type=$2 body=$3 folder="$scratch/java/${1//.//}"
  mkdir -p "$folder"
  printf 'package %s;\n%s\n' "$package" "$body" > "$folder/$type.java"
}
stub com.watabou.noosa Game 'public class Game {
    public static float elapsed;
    public static void reportException(Exception e) { throw new RuntimeException(e); }
}'
stub com.watabou.utils Bundle 'public class Bundle {
    public void put(String key, Object val) {}
    public int getInt(String key) { return 0; }
    public boolean getBoolean(String key) { return false; }
    public <T extends Enum<T>> T getEnum(String key, Class<T> type) { return null; }
}'
stub com.watabou.utils Bundlable 'public interface Bundlable {
    void storeInBundle(Bundle b); void restoreFromBundle(Bundle b);
}'
stub "$game.levels" Level 'public class Level {}'
stub "$game.levels.painters" Painter "import $game.levels.Level;
    import com.watabou.utils.Rect;
    public class Painter { public static void fill(Level l, Rect r, int terrain) {} }"
stub "$game.levels.rooms" Fixture "import $game.levels.Level;
    public class Fixture extends Room {
        public int lo = 5, hi = 10, cap = 16, side = 4, id, type = -1;
        public int minWidth() { return lo; } public int minHeight() { return lo; }
        public int maxWidth() { return hi; } public int maxHeight() { return hi; }
        public int maxConnections(int d) { return d == ALL ? cap : side; }
        public void paint(Level l) {}
    }"
stub "$game.levels.rooms.standard" StandardRoom "import $game.levels.rooms.Fixture;
    public class StandardRoom extends Fixture {
        public int factor = 1;
        public int sizeFactor() { return factor; }
        public int connectionWeight() { return factor * factor; }
    }"
stub "$game.levels.rooms.secret" SecretRoom "public class SecretRoom extends $game.levels.rooms.Fixture {}"
stub "$game.levels.rooms.special" ShopRoom "public class ShopRoom extends $game.levels.rooms.Fixture {}"
stub "$game.levels.rooms.connection" ConnectionRoom "import $game.levels.rooms.Fixture;
    import com.watabou.utils.Random;
    public class ConnectionRoom extends Fixture {
        public static int depth;
        private static final float[][] WEIGHTS = {
          {20,1,0,2,2,1}, {0,0,22,3,0,0}, {12,0,0,5,5,3}, {0,0,18,3,3,1}, {15,4,0,2,3,2} };
        public ConnectionRoom() { lo = 3; }
        public static ConnectionRoom createRoom() {
            ConnectionRoom r = new ConnectionRoom();
            r.type = Random.chances(depth == 21 ? new float[]{20,0,0,0,0,0} : WEIGHTS[(depth-1)/5]);
            if (r.type >= 4) r.lo = 5;
            return r;
        }
    }"
stub "$game.levels.rooms.connection" MazeConnectionRoom 'public class MazeConnectionRoom extends ConnectionRoom {
    public MazeConnectionRoom() { cap = side = 2; type = 6; }
}'

find "$scratch/java" -name '*.java' -print > "$scratch/sources.txt"
javac -d "$scratch/classes" @"$scratch/sources.txt" tools/floor-layout/Oracle.java
java -cp "$scratch/classes" Oracle > "$scratch/java.txt"
"${CXX:-c++}" -std=c++17 -O2 -ffp-contract=off -Isource tools/floor-layout/native.cpp -o "$scratch/native"
"$scratch/native" > "$scratch/native.txt"
diff -u "$scratch/java.txt" "$scratch/native.txt"
printf '1,000 Loop/FigureEight fixtures match v4.0.2 (room geometry, type, link order, RNG state)\n'
printf '500 Patch fixtures match v4.0.2 (cell masks and RNG state)\n'
printf '100 Maze fixtures match v4.0.2 (wall masks and RNG state)\n'

# Test the regular-floor prefix separately: real upstream factories,
# constructors, budgets and geometry, with painting and game services omitted.
level_src="core/src/main/java/$dir/levels"
standard_src="$level_src/rooms/standard"
while IFS= read -r path; do
  relative=${path#"$level_src/"}
  mkdir -p "$scratch/java/$dir/levels/$(dirname "$relative")"
  git -C "$upstream" show "$ref:$path" | awk -f tools/floor-layout/room-geometry.awk > "$scratch/java/$dir/levels/$relative"
done < <(git -C "$upstream" ls-tree -r --name-only "$ref" "$standard_src" | grep '/[^/]*Room\.java$' | grep -vE '/ImpShopRoom\.java$|/quest/')
stub "$game" Dungeon 'public class Dungeon { public static int depth; }'
stub com.watabou.utils Reflection 'public class Reflection {
    public static <T> T newInstance(Class<T> c) {
        try { return c.getDeclaredConstructor().newInstance(); }
        catch (ReflectiveOperationException e) { throw new RuntimeException(e); }
    }
}'
{
  printf 'package %s.levels;\n' "$game"
  printf 'import %s.Dungeon;\n' "$game"
  printf 'import %s.levels.rooms.standard.StandardRoom;\n' "$game"
  printf 'import %s.levels.rooms.standard.entrance.EntranceRoom;\n' "$game"
  printf 'import %s.levels.rooms.standard.exit.ExitRoom;\n' "$game"
  printf 'import com.watabou.utils.Random;\nimport java.util.ArrayList;\npublic class RoomBudget {\n'
  for region in Sewer Prison Caves City Halls; do
    git -C "$upstream" show "$ref:$level_src/${region}Level.java" |
      sed -n '/protected int standardRooms(boolean forceMax)/,/^[[:space:]]*}/p' |
      sed "s/protected int standardRooms/static int standardRooms$region/"
  done
  printf '%s\n' 'static int standardRooms(boolean large) {' 'switch ((Dungeon.depth-1)/5) {'
  printf '%s\n' 'case 0: return standardRoomsSewer(large);' 'case 1: return standardRoomsPrison(large);' \
    'case 2: return standardRoomsCaves(large);' 'case 3: return standardRoomsCity(large);' 'default: return standardRoomsHalls(large);' '} }'
  printf '%s\n' 'public static ArrayList<StandardRoom> initRooms(boolean large) {'
  git -C "$upstream" show "$ref:$level_src/RegularLevel.java" |
    awk '/protected ArrayList<Room> initRooms\(/ { prefix=1; next } /if \(Dungeon.shopOnLevel\(\)\)/ { exit } prefix' |
    sed -e 's/ArrayList<Room>/ArrayList<StandardRoom>/' -e 's/roomEntrance = //; s/roomExit = //; s/feeling == Feeling.LARGE/large/g'
  printf '%s\n' 'return initRooms; } }'
} > "$scratch/java/$dir/levels/RoomBudget.java"
find "$scratch/java" -name '*.java' -print > "$scratch/sources.txt"
javac -d "$scratch/classes" @"$scratch/sources.txt" tools/floor-layout/RoomOracle.java
java -cp "$scratch/classes" RoomOracle > "$scratch/java-rooms.txt"
"${CXX:-c++}" -std=c++17 -O2 -ffp-contract=off -Isource tools/floor-layout/room-native.cpp -o "$scratch/room-native"
"$scratch/room-native" > "$scratch/native-rooms.txt"
diff -u "$scratch/java-rooms.txt" "$scratch/native-rooms.txt"
printf '2,000 regular-floor prefixes match v4.0.2 (ordered room types, sizes, door candidates and RNG state)\n'

# Use the original painter methods with a small in-memory level. This pass
# replaces the layout fixtures; it does not run the full RegularPainter.
for file in painters/Painter rooms/connection/{Connection,Tunnel,Bridge,Perimeter,Walkway,RingTunnel,RingBridge,MazeConnection}Room rooms/standard/{Pillars,Segmented,CircleWall}Room; do
  git -C "$upstream" show "$ref:$level_src/$file.java" > "$scratch/java/$dir/levels/$file.java"
done
git -C "$upstream" show "$ref:SPD-classes/src/main/java/$utils/PathFinder.java" > "$scratch/java/$utils/PathFinder.java"
stub com.watabou.utils BArray 'public class BArray {
    public static void setFalse(boolean[] a) { java.util.Arrays.fill(a, false); }
}'
stub "$game.levels" Level 'public class Level {
    public int[] map = new int[32*32];
    public int width() { return 32; }
    public int tunnelTile() { return Terrain.EMPTY; }
    public int pointToCell(com.watabou.utils.Point p) { return p.x+p.y*width(); }
}'
stub "$game.levels" Terrain 'public class Terrain {
    public static final int WALL = 0, EMPTY = 1, CHASM = 2;
}'
find "$scratch/java" -name '*.java' -print > "$scratch/sources.txt"
javac -d "$scratch/classes" @"$scratch/sources.txt" tools/floor-layout/PainterOracle.java
java -cp "$scratch/classes" PainterOracle > "$scratch/java-painter.txt"
"${CXX:-c++}" -std=c++17 -O2 -ffp-contract=off -Isource tools/floor-layout/painter-native.cpp -o "$scratch/painter-native"
"$scratch/painter-native" > "$scratch/native-painter.txt"
diff -u "$scratch/java-painter.txt" "$scratch/native-painter.txt"
printf '1,800 room painters match v4.0.2 (six connectors, three standard rooms, terrain and RNG state)\n'
