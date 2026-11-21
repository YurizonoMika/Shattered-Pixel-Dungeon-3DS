import com.shatteredpixel.shatteredpixeldungeon.levels.builders.*;
import com.shatteredpixel.shatteredpixeldungeon.levels.Patch;
import com.shatteredpixel.shatteredpixeldungeon.levels.features.Maze;
import com.shatteredpixel.shatteredpixeldungeon.levels.rooms.*;
import com.shatteredpixel.shatteredpixeldungeon.levels.rooms.connection.ConnectionRoom;
import com.shatteredpixel.shatteredpixeldungeon.levels.rooms.secret.SecretRoom;
import com.shatteredpixel.shatteredpixeldungeon.levels.rooms.special.ShopRoom;
import com.shatteredpixel.shatteredpixeldungeon.levels.rooms.standard.StandardRoom;
import com.watabou.utils.Random;
import java.util.ArrayList;

public class Oracle {
    static final class Entrance extends StandardRoom {
        @Override public boolean isEntrance() { return true; }
    }
    static final class Exit extends StandardRoom {
        @Override public boolean isExit() { return true; }
    }

    public static void main(String[] args) {
        for (int i = 0; i < 1000; i++) {
            int depth = 1 + i % 20 + i % 20 / 4;
            Random.pushGenerator(0x89abcdef01234567L * (i + 1));
            for (int j = 0; j < depth; j++) Random.Long();
            long seed = Random.Long();
            Random.popGenerator();
            Random.pushGenerator(seed);
            Builder builder = Random.Int(2) == 0 ? new LoopBuilder().setLoopShape(2,
                Random.Float(0, 0.65f), Random.Float(0, 0.5f)) : new FigureEightBuilder().setLoopShape(2,
                Random.Float(0.3f, 0.8f), 0);
            ConnectionRoom.depth = depth;
            ArrayList<Room> init = new ArrayList<>();
            init.add(new Entrance());
            init.add(new Exit());
            for (int j = 0; j < 4 + i % 8; j++) {
                StandardRoom r = new StandardRoom();
                r.factor = j == 0 ? 2 : j == 1 && i % 5 == 0 ? 3 : 1;
                r.lo = r.factor == 1 ? 4 : r.factor == 2 ? 10 : 14;
                r.hi = r.factor == 1 ? 10 : r.factor == 2 ? 14 : 18;
                init.add(r);
            }
            if (i % 3 == 0) {
                ShopRoom r = new ShopRoom(); r.lo = 7; r.cap = r.side = 1; init.add(r);
            }
            Fixture leaf = new Fixture(); leaf.cap = leaf.side = 1; init.add(leaf);
            SecretRoom secret = new SecretRoom(); secret.cap = secret.side = 1; init.add(secret);
            for (int j = 0; j < init.size(); j++) ((Fixture)init.get(j)).id = j;
            Random.shuffle(init);
            ArrayList<Room> result = null;
            int attempt;
            for (attempt = 1; attempt <= 256; attempt++) {
                for (Room r : init) { r.connected.clear(); r.neigbours.clear(); }
                result = builder.build(new ArrayList<>(init));
                if (result != null) break;
            }
            if (result == null) throw new AssertionError("exhausted attempts for " + i);
            for (int j = init.size(); j < result.size(); j++) ((Fixture)result.get(j)).id = j;
            System.out.printf("%d %d %d %016x%n", i, attempt, result.size(), Random.Long());
            for (Room r : result) {
                System.out.printf("%d %d %d %d %d %d:", ((Fixture)r).id, ((Fixture)r).type,
                    r.left, r.top, r.right, r.bottom);
                for (Room n : r.connected.keySet()) System.out.printf(" %d", ((Fixture)n).id);
                System.out.println();
            }
            Random.popGenerator();
        }
        for (int i = 0; i < 500; i++) {
            Random.pushGenerator(0x713abc0012345678L * (i + 1));
            boolean[] cells = Patch.generate(2+i%63, 2+(i*17)%63, (i%21)/20f, i%7, i%3 != 0);
            for (boolean c : cells) System.out.print(c ? '1' : '0');
            System.out.printf(" %016x%n", Random.Long());
            Random.popGenerator();
        }
        for (int i = 0; i < 100; i++) {
            Random.pushGenerator(0x0123456789abcd2fL * (i + 1));
            Fixture r = new Fixture();
            int w = 3+i%8, h = 3+(i*5)%8;
            r.set(0,0,w-1,h-1);
            r.connected.put(new Fixture(), new Room.Door(0,1+i%(h-2)));
            r.connected.put(new Fixture(), new Room.Door(w-1,1+(i*3)%(h-2)));
            boolean[][] cells = Maze.generate(r);
            for (int y = 0; y < h; y++) for (int x = 0; x < w; x++)
                System.out.print(cells[x][y] ? '1' : '0');
            System.out.printf(" %016x%n", Random.Long());
            Random.popGenerator();
        }
    }
}
