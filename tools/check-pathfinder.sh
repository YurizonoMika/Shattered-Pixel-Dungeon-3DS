#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
upstream=${1:-../spd-upstream}
src="$upstream/SPD-classes/src/main/java/com/watabou/utils"
scratch=$(mktemp -d)
trap 'rm -f "$scratch"/*.class "$scratch"/native "$scratch"/*.txt "$scratch"/Check.java; rm -f "$scratch"/com/watabou/utils/*.class; rmdir "$scratch"/com/watabou/utils "$scratch"/com/watabou "$scratch"/com "$scratch" 2>/dev/null || true' EXIT

cat > "$scratch/Check.java" <<'JAVA'
import com.watabou.utils.PathFinder;
public class Check {
    static int seed = 42;
    static int draw(int bound) {
        seed ^= seed << 13;
        seed ^= seed >>> 17;
        seed ^= seed << 5;
        return Integer.remainderUnsigned(seed, bound);
    }
    public static void main(String[] args) {
        PathFinder.setMapSize(19, 17);
        for (int trial = 0; trial < 10000; trial++) {
            boolean[] pass = new boolean[19 * 17];
            int density = 10 + draw(85);
            for (int i = 0; i < pass.length; i++) {
                int x = i % 19, y = i / 19;
                if (x > 0 && x < 18 && y > 0 && y < 16) pass[i] = draw(100) < density;
            }
            int a = (1 + draw(15)) * 19 + 1 + draw(17);
            int b = (1 + draw(15)) * 19 + 1 + draw(17);
            pass[a] = true;
            System.out.printf("%d %d %d%n", PathFinder.getStep(a, b, pass),
                PathFinder.getStepBack(a, b, 4, pass.clone(), false),
                PathFinder.getStepBack(a, b, 8, pass.clone(), true));
        }
    }
}
JAVA
javac -d "$scratch" "$src/PathFinder.java" "$src/BArray.java" "$scratch/Check.java"
java -cp "$scratch" Check > "$scratch/java.txt"
"${CXX:-c++}" -std=c++17 -O2 -Isource -x c++ -o "$scratch/native" - <<'CPP'
#include "pathfinder.h"
#include <cstdio>
uint32_t seed = 42;
int draw(int bound) {
  seed ^= seed << 13;
  seed ^= seed >> 17;
  seed ^= seed << 5;
  return seed % bound;
}
int main() {
  PathFinder<19, 17> path;
  for (int trial = 0; trial < 10000; ++trial) {
    bool pass[19 * 17]{};
    int density = 10 + draw(85);
    for (int i = 0; i < 19 * 17; ++i) {
      int x = i % 19, y = i / 19;
      if (x > 0 && x < 18 && y > 0 && y < 16) pass[i] = draw(100) < density;
    }
    int a = (1 + draw(15)) * 19;
    a += 1 + draw(17);
    int b = (1 + draw(15)) * 19;
    b += 1 + draw(17);
    pass[a] = true;
    auto valid = [&](int i) { return pass[i]; };
    int step = path.step(a, b, valid);
    int terror = path.retreat(a, b, 4, false, valid);
    int flee = path.retreat(a, b, 8, true, valid);
    std::printf("%d %d %d\n", step, terror, flee);
  }
}
CPP
"$scratch/native" > "$scratch/native.txt"
diff -u "$scratch/java.txt" "$scratch/native.txt"
printf '30,000 pathfinder results match upstream\n'
