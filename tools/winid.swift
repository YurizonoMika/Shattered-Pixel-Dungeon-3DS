// Print the window id and bounds of the Azahar window:
//   id x y w h
import Cocoa

guard let list = CGWindowListCopyWindowInfo([.optionOnScreenOnly, .excludeDesktopElements], kCGNullWindowID) as? [[String: Any]] else {
  exit(2)
}
for w in list {
  let owner = (w[kCGWindowOwnerName as String] as? String ?? "").lowercased()
  if owner.contains("azahar") {
    let wid = w[kCGWindowNumber as String] as? Int ?? 0
    let b = w[kCGWindowBounds as String] as? [String: Any] ?? [:]
    let x = b["X"] as? Double ?? 0
    let y = b["Y"] as? Double ?? 0
    let ww = b["Width"] as? Double ?? 0
    let hh = b["Height"] as? Double ?? 0
    print("\(wid) \(Int(x)) \(Int(y)) \(Int(ww)) \(Int(hh))")
    exit(0)
  }
}
exit(1)
