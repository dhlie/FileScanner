package d.hl.filescann;

/**
 * Created by dhl on 17-8-27.
 */

public class FileScanner {

    static {
        System.loadLibrary("scanner");
    }

    interface ScanCallback {
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

        String path;
        long size;
        long modifyTime;
    }

    /**
     * 初始化扫描器
     * @param suf           :要扫描的文件后缀(不区分大小写,不带'.'),空数组表示查找所有文件,不能为null
     * @param thdCount      :扫描线程数
     * @param depth         :扫描目录深度(-1时扫描所有目录)
     * @param getFileDetail :是否返回文件大小,修改日期(默认只返回文件路径)
     */
    public void initScanner(String[] suf, int thdCount, int depth, boolean getFileDetail) {
        if (suf == null || thdCount < 1) {
            throw new RuntimeException("参数错误");
        }
        nativeInitScanner(suf, thdCount, depth, getFileDetail);
    }

    public void setScanCallback(ScanCallback cb) {
        if (cb != null) {
            nativeSetCallback(cb);
        }
    }

    /**
     *  扫描路径
     * @param path :要扫描的路径数组
     */
    public void startScan(String path[]) {
        if (path != null && path.length > 0) {
            nativeStartScan(path);
        }
    }

    public void stopScan() {
        nativeStopScan();
    }

    public void recycle() {
        nativeRrecycle();
    }


    private native void nativeInitScanner(String[] suf, int thdCount, int depth, boolean getFileDetail);
    private native void nativeSetCallback(ScanCallback callback);
    private native void nativeStartScan(String path[]);
    private native void nativeStopScan();
    private native void nativeRrecycle();
}
