-keep public class com.dhl.filescanner.FileScanner {
    *;
}

-keep public class * implements com.dhl.filescanner.FileScanner$ScanCallback {
    *;
}