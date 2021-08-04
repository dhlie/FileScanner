## FileScanner

C 实现的 Android 文件扫描器

1． 非递归扫描，性能更高

2． 支持设置扫描线程数

3． 支持设置扫描目录层级

4． 支持设置是否扫描隐藏目录

5． 支持设置是否扫描包含 .nomedia 的目录



使用方法：

```java
    //要扫描的文件类型，不区分大小写
    String[] ext = new String[] { "doc", "docx", "xls", "xlsx", "jpg", "jpeg", "png", "bmp", "gif", "mp3", "mp4", "avi", "rmvb" };
	//nomedia 目录中过滤掉的文件类型， 为空时不过滤
	String[] filteredExt = new String[]{"jpg", "jpeg", "png", "bmp", "gif"}
    //扫描线程数
    int threadCount = 4;
    //扫描目录层级, -1 扫描所有目录
    int scanDepth = -1;
    //扫描目录
    String[] scanPaths = new String[] { Environment.getExternalStorageDirectory().getAbsolutePath() };

    FileScanner fileScanner = new FileScanner();
    fileScanner.setScanParams(ext, filteredExt, threadCount, scanDepth, true);
    //是否扫描隐藏目录
    fileScanner.setScanHiddenEnable(false);
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



混淆：library 库自带混淆规则，并且会自动导入，正常情况下无需手动导入