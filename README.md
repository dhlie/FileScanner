## FileScanner

Android 文件扫描器

使用方法：

```
    //要扫描的文件后缀，不区分大小写
    String[] ext = new String[] { "jpg", "jpeg", "png", "bmp", "gif", "mp3", "mp4", "avi", "rmvb" };
    //扫描线程数
    int threadCount = 4;
    //扫描目录层级, -1 扫描所有目录
    int scanDepth = -1;
    //扫描目录
    String[] scanPaths = new String[] { Environment.getExternalStorageDirectory().getAbsolutePath() };

    FileScanner fileScanner = new FileScanner();
    fileScanner.setScanParams(ext, threadCount, scanDepth, true);
    //是否扫描隐藏目录
    fileScanner.setHideDirScanEnable(false);
    //是否扫描 .nomedia 目录
    fileScanner.setNoMediaDirScanEnable(false);
    fileScanner.setScanPath(scanPaths);
    fileScanner.startScan(new AbstractScanCallback() {
      @Override
      public void onScanStart() {
        //start scan
      }

      @Override
      public void onScanFinish(final List<FileScanner.FindItem> files, final boolean isCancel) {
        //finish scan
      }
    });
```



混淆配置：

```
-keep public class d.hl.filescan.FileScanner {
    *;
}

-keep public class * implements d.hl.filescan.FileScanner$ScanCallback {
    *;
}
```