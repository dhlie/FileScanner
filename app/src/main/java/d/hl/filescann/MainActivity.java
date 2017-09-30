package d.hl.filescann;

import android.app.Activity;
import android.os.Bundle;
import android.os.Environment;
import android.text.TextUtils;
import android.util.Log;
import android.view.View;
import android.view.ViewGroup;
import android.widget.BaseAdapter;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.ListView;
import android.widget.TextView;

import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.List;

public class MainActivity extends Activity implements View.OnClickListener {

    private TextView mTVINfo;
    private EditText mETThread, mETDepth;
    private CheckBox mCBDetail;

    private MyAdapter mAdapter = new MyAdapter();
    private FileScanner mFileScanner;
    private String[] mSuffixes;
    private long mStartTime;
    private int mThreadCount, mScanDepth;
    private boolean mNeedDetail;
    private String mScanPath[];

    private FileScanner.ScanCallback callback = new AbstractScanCallback() {
        @Override
        public void onScanStart() {
            mStartTime = System.currentTimeMillis();
            runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    mTVINfo.setText("扫描中...  total:0 time:0 thd:"+ mThreadCount +"  dp:"+ mScanDepth);
                    mAdapter.changeData(null);
                }
            });
        }

        @Override
        public void onScanFinish(final List<FileScanner.FindItem> files, final boolean isCancel) {
            final long time = System.currentTimeMillis() - mStartTime;
            Log.i("Scanner", "Scanner java callback:  onFinish  time:"+time + "  count:"+files.size());
            runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    mTVINfo.setText("total:"+files.size() + " time:"+time+" thd:"+ mThreadCount +"  dp:"+ mScanDepth);
                    mAdapter.changeData(files);
                }
            });
        }
    };

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        Button btnStart = (Button) findViewById(R.id.tv_start);
        Button btnStop = (Button) findViewById(R.id.tv_stop);
        mTVINfo = (TextView) findViewById(R.id.tv_info);
        ListView lvList = (ListView) findViewById(R.id.lv);
        Button btnSet = (Button) findViewById(R.id.tv_reset);
        mETThread = (EditText) findViewById(R.id.et_thd);
        mETDepth = (EditText) findViewById(R.id.et_depth);
        mCBDetail = (CheckBox) findViewById(R.id.cb_detail);

        btnStart.setOnClickListener(this);
        btnStop.setOnClickListener(this);
        btnSet.setOnClickListener(this);
        lvList.setAdapter(mAdapter);

        int precessorNum = Runtime.getRuntime().availableProcessors();
        precessorNum = precessorNum > 4 ? 4 : precessorNum;
        mThreadCount = precessorNum;
        mScanDepth = -1;//全盘扫描
        mNeedDetail = false;//是否获取文件详情
        mFileScanner = new FileScanner();
        mSuffixes = new String[]{};//查找所有文件
        //mSuffixes = new String[]{"mp3", "mp4", "avi", "rmvb"};
        //mSuffixes = new String[]{"jpg", "jpeg", "png", "bmp", "gif"};
        //mSuffixes = new String[]{"EBK2","EBK3","TXT","EPUB","CHM","UMD","PDF", "OPUB", "DOC", "DOCX",
        //        "WPS", "XLS", "XLSX", "ET", "PPT", "PPTX", "DPS"};
        mSuffixes = new String[]{"jpg", "jpeg", "png", "bmp", "gif", "mp3", "mp4", "avi", "rmvb", "wmv", "wma", "flav", "wav", "ogg", "mp2", "m4a", "au", "aac", "3gp", "3g2", "asf", "flv", "mov", "rm", "swf", "mpg", "EBK2","EBK3","TXT","EPUB","CHM","UMD","PDF", "OPUB", "DOC", "DOCX",
                "WPS", "XLS", "XLSX", "ET", "PPT", "PPTX", "DPS"};
        mFileScanner.initScanner(mSuffixes, mThreadCount, mScanDepth, mNeedDetail);
        mFileScanner.setScanCallback(callback);
        String sdPath = Environment.getExternalStorageDirectory().getAbsolutePath();
        mScanPath = new String[] {sdPath};
        //mScanPath = new String[] {sdPath+"/Android/", sdPath+"/DCIM/"};
        mFileScanner.startScan(mScanPath);

        mETDepth.setText(""+ mScanDepth);
        mETThread.setText(""+ mThreadCount);
        mCBDetail.setChecked(mNeedDetail);
    }

    @Override
    public void onClick(View view) {
        switch (view.getId()) {
            case R.id.tv_start:
                mFileScanner.startScan(mScanPath);
                break;
            case R.id.tv_stop:
                if (mFileScanner != null) mFileScanner.stopScan();
                break;
            case R.id.tv_reset:
                String thd = mETThread.getText().toString();
                mThreadCount = Integer.parseInt(TextUtils.isEmpty(thd) ? "1" : thd);
                String deep = mETDepth.getText().toString();
                mScanDepth = Integer.parseInt(TextUtils.isEmpty(deep) ? "-1" : deep);
                mNeedDetail = mCBDetail.isChecked();
                mFileScanner.initScanner(mSuffixes, mThreadCount, mScanDepth, mNeedDetail);
                mETDepth.setText(""+ mScanDepth);
                mETThread.setText(""+ mThreadCount);
                break;
        }
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        mFileScanner.recycle();
    }

    private class MyAdapter extends BaseAdapter {

        private List<FileScanner.FindItem> mData;
        private SimpleDateFormat mFormat = new SimpleDateFormat("yyyy-MM-dd HH:mm");
        private Date mDate = new Date();

        public void changeData(List<FileScanner.FindItem> items) {
            mData = items;
            notifyDataSetChanged();
        }

        @Override
        public int getCount() {
            return mData == null ? 0 : mData.size();
        }

        @Override
        public View getView(int i, View view, ViewGroup viewGroup) {
            TextView tv;
            if (view == null) {
                tv = new TextView(MainActivity.this);
                tv.setPadding(4, 4, 4, 4);
            } else {
                tv = (TextView) view;
            }
            FileScanner.FindItem item = mData.get(i);
            mDate.setTime(item.modifyTime*1000);
            if (mNeedDetail) {
                tv.setText(item.path.substring(item.path.lastIndexOf('/')+1) + "   "+mFormat.format(mDate) + "   size:"+item.size/1024+"KB");
            } else {
                tv.setText(item.path.substring(item.path.lastIndexOf('/')+1));
            }
            return tv;
        }

        @Override
        public Object getItem(int i) {
            return null;
        }

        @Override
        public long getItemId(int i) {
            return 0;
        }
    }
}
