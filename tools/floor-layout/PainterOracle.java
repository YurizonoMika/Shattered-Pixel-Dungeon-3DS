import com.shatteredpixel.shatteredpixeldungeon.levels.Level;
import com.shatteredpixel.shatteredpixeldungeon.levels.rooms.Room;
import com.shatteredpixel.shatteredpixeldungeon.levels.rooms.connection.*;
import com.shatteredpixel.shatteredpixeldungeon.levels.rooms.standard.*;
import com.watabou.utils.PathFinder;
import com.watabou.utils.Random;

public class PainterOracle {
    public static void main(String[] args) {
        PathFinder.setMapSize(32, 32);
        for (int i = 0; i < 1800; ++i) {
            int type = i % 9, sample = i / 9;
            Room room;
            switch (type) {
                case 0: room = new TunnelRoom(); break;
                case 1: room = new BridgeRoom(); break;
                case 2: room = new PerimeterRoom(); break;
                case 3: room = new WalkwayRoom(); break;
                case 4: room = new RingTunnelRoom(); break;
                case 5: room = new RingBridgeRoom(); break;
                case 6: room = new PillarsRoom(); break;
                case 7: room = new SegmentedRoom(); break;
                default: room = new CircleWallRoom(); break;
            }
            int cat = type == 8 ? 1 + sample % 2 : sample % 3;
            if (room instanceof StandardRoom)
                ((StandardRoom)room).sizeCat = StandardRoom.SizeCategory.values()[cat];
            int lo = room.minWidth(), hi = room.maxWidth();
            int w = lo + sample % (hi-lo+1), h = lo + sample / (hi-lo+1) % (hi-lo+1);
            room.set(2, 3, w+1, h+2);
            for (int j = 0; j < 2 + sample % 5; ++j) {
                int side = (sample + j) % 4;
                int x = 3 + (sample*7+j*3) % (w-2), y = 4 + (sample*3+j*5) % (h-2);
                if (side == 0) x = room.left;
                if (side == 1) y = room.top;
                if (side == 2) x = room.right;
                if (side == 3) y = room.bottom;
                room.connected.put(new com.shatteredpixel.shatteredpixeldungeon.levels.rooms.Fixture(), new Room.Door(x,y));
            }
            Level level = new Level();
            Random.pushGenerator(0x5e2fd32137bcee91L * (i+1));
            room.paint(level);
            System.out.printf("%d %dx%d ", i, w, h);
            for (int y = room.top; y <= room.bottom; ++y)
                for (int x = room.left; x <= room.right; ++x)
                    System.out.print(level.map[x+y*level.width()]);
            System.out.printf(" %016x%n", Random.Long());
            Random.popGenerator();
        }
    }
}
