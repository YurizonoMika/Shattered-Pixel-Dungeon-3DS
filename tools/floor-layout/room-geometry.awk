# Retain the upstream class hierarchy, factories and geometry methods. Strip
# painting and gameplay methods so the host oracle needs no game runtime.
/^package / { print; next }
/^import (java\.util\.ArrayList|com\.watabou\.utils\.(Point|Rect|Random|Reflection));/ { print; next }
/^import com\.shatteredpixel\.shatteredpixeldungeon\.(Dungeon|levels\.rooms\.(Room|standard\.[A-Za-z.]+));/ { print; next }
/^public (abstract )?class / {
    print
    if ($0 ~ /class StandardRoom /) print "    public void paint(com.shatteredpixel.shatteredpixeldungeon.levels.Level level) {}"
    next
}
/^\tprivate static ArrayList<Class/ { factory = 1 }
factory { print; next }
/^\tpublic enum SizeCategory/ { body = 1 }
/^\tpublic SizeCategory sizeCat;/ || /^\t\{ setSizeCat\(\); \}/ { print; next }
/^\t(public|protected) .* (minWidth|maxWidth|minHeight|maxHeight|maxConnections|canConnect|resize|sizeCatProbs|setSizeCat|sizeFactor|connectionWeight|isEntrance|isExit)\(/ { body = 1 }
body { print; if ($0 ~ /^\t}/ || $0 ~ /\{.*\}[[:space:]]*$/) body = 0; next }
/^}$/ { print }
