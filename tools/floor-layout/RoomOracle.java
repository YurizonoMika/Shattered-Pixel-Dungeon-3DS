import com.shatteredpixel.shatteredpixeldungeon.Dungeon;
import com.shatteredpixel.shatteredpixeldungeon.levels.RoomBudget;
import com.shatteredpixel.shatteredpixeldungeon.levels.rooms.Room;
import com.shatteredpixel.shatteredpixeldungeon.levels.rooms.standard.StandardRoom;
import com.shatteredpixel.shatteredpixeldungeon.levels.rooms.standard.entrance.EntranceRoom;
import com.shatteredpixel.shatteredpixeldungeon.levels.rooms.standard.exit.ExitRoom;
import com.watabou.utils.Point;
import com.watabou.utils.Random;
import java.util.ArrayList;

public class RoomOracle {
    public static void main(String[] args) {
        for (int i = 0; i < 2000; ++i) {
            int depth = 1 + i % 20 + i % 20 / 4;
            boolean large = (i / 20) % 2 != 0;
            Random.pushGenerator(0xb13fcacf41234561L * (i + 1));
            Dungeon.depth = depth;
            ArrayList<StandardRoom> rooms = RoomBudget.initRooms(large);
            System.out.printf("%d %d %s %d %016x%n", i, depth, large, rooms.size(), Random.Long());
            for (StandardRoom r : rooms) {
                String type = r.getClass().getSimpleName().replaceFirst("(Entrance|Exit)?Room$", "");
                System.out.printf("%s %d %d %d %d %d %d %d", type, r.sizeFactor(),
                    r.minWidth(), r.maxWidth(), r.minHeight(), r.maxHeight(),
                    r.maxConnections(Room.ALL), r.maxConnections(Room.TOP));
                for (int j = 0; j < 3; ++j) {
                    if (!r.setSizeWithLimit(r.minWidth() + j, Math.max(r.minHeight(), r.maxHeight() - j)))
                        throw new AssertionError("Invalid fixture dimensions");
                    r.setPos(0, 0);
                    System.out.printf(" %dx%d:", r.width(), r.height());
                    for (int x = 0; x < r.width(); ++x) System.out.print(r.canConnect(new Point(x, 0)) ? '1' : '0');
                }
                System.out.println();
            }
            System.out.printf("%016x%n", Random.Long());
            Random.popGenerator();
        }
    }
}
