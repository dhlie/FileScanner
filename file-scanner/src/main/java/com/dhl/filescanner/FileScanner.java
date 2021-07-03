package com.dhl.filescanner;

/**
 * Created by dhl on 17-8-27.
 */

public class FileScanner {

  static {
    System.loadLibrary("fileScanner");
  }

  public interface ScanCallback {
    void onStart();

    void onFind(long threadId, String path, long size, long modify);

    void onFinish(boolean isCancel);
  }

  public static class FindItem {

    public FindItem(String path, long size, long modifyTime) {
      this.path = path;
      this.size = size;
      this.modifyTime = modifyTime;
    }

    public String path;
    public long size;
    public long modifyTime;

  }

  private long mHandle;

  public FileScanner() {
    mHandle = nativeCreate();
  }

  @Override
  protected void finalize() throws Throwable {
    super.finalize();

    release();
  }

  public synchronized void release() {
    if (mHandle != 0) {
      nativeRelease(mHandle);
      mHandle = 0;
    }
  }

  /**
   * 初始化扫描器
   *
   * @param suf           :要扫描的文件后缀(不区分大小写,不带'.'), null 或 空数组表示查找所有文件
   * @param filteredNoMediaSuf: nomedia 目录过滤掉的文件类型, null 或 空数组表示不过滤,
   *                          nomedia 目录是当前目录或任何父级目录中有 .nomedia 文件的目录
   * @param thdCount      :扫描线程数
   * @param depth         :扫描目录深度(-1时扫描所有目录)
   * @param getFileDetail :是否返回文件大小,修改日期(默认只返回文件路径)
   */
  public void setScanParams(String[] suf, String[] filteredNoMediaSuf, int thdCount, int depth, boolean getFileDetail) {
    if (thdCount < 1) {
      throw new RuntimeException("参数错误");
    }
    nativeSetScanParams(mHandle, suf, filteredNoMediaSuf, thdCount, depth, getFileDetail);
  }

  /**
   * 是否扫描隐藏目录, 默认 true
   *
   * @param scanHidden
   */
  public void setScanHiddenEnable(boolean scanHidden) {
    nativeSetScanHiddenEnable(mHandle, scanHidden);
  }

  /**
   * 设置扫描路径
   * @param path 要扫描的路径数组
   */
  public void setScanPath(String[] path) {
    if (path != null && path.length > 0) {
      nativeSetScanPath(mHandle, path);
    }
  }

  /**
   * 开始扫描
   * @param callback
   */
  public void startScan(ScanCallback callback) {
    if (callback != null) {
      nativeStartScan(mHandle, callback);
    }
  }

  public void stopScan() {
    nativeStopScan(mHandle);
  }

  private native long nativeCreate();

  private native void nativeRelease(long handle);

  private native void nativeSetScanParams(long handle, String[] suf, String[] filteredNoMediaSuf, int thdCount, int depth, boolean getFileDetail);

  private native void nativeSetScanHiddenEnable(long handle, boolean enable);

  private native void nativeSetScanPath(long handle, String path[]);

  private native int nativeStartScan(long handle, ScanCallback callback);

  private native void nativeStopScan(long handle);

}