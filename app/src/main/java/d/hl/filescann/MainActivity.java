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
import android.widget.EditText;
import android.widget.ListView;
import android.widget.TextView;

import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Date;
import java.util.List;

public class MainActivity extends Activity implements View.OnClickListener {

    private Button btnStart, btnStop, btnSet;
    private TextView tvINfo;
    private ListView lvList;
    private EditText etThread, etDepth;

    private MyAdapter mAdapter = new MyAdapter();
    private FileScanner fileScanner;
    private String[] suf;
    private List<FindItem> files = new ArrayList<>(100);
    private long startTime;
    private int thread = 1, depth = -1;
    private boolean detail = true;
    private String path[];

    private FileScanner.ScanCallback callback = new FileScanner.ScanCallback() {
        @Override
        public void onStart() {
            runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    tvINfo.setText("文件数:0 thd:"+thread+"  dp:"+depth);
                    mAdapter.changeData(null);
                }
            });
            startTime = System.currentTimeMillis();
            files = new ArrayList<>(100);
        }

        @Override
        public void onFind(String path, long size, long modify) {
            if (detail && path.endsWith(".txt") && size < 10240) return;
            FindItem item = new FindItem();
            item.path = path;
            item.size = size;
            item.modifyTime = modify;
            files.add(item);
        }

        @Override
        public void onCancel() {
            runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    tvINfo.setText(""+files.size());
                    mAdapter.changeData(files);
                }
            });
        }

        @Override
        public void onFinish() {
            final long time = System.currentTimeMillis() - startTime;
            Log.i("Scanner", "Scanner java callback:  onFinish  time:"+time);
            runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    tvINfo.setText("文件数:"+files.size() + " 用时:"+time+" thd:"+thread+"  dp:"+depth);
                    mAdapter.changeData(files);
                }
            });
        }
    };

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        btnStart = (Button) findViewById(R.id.tv_start);
        btnStop = (Button) findViewById(R.id.tv_stop);
        tvINfo = (TextView) findViewById(R.id.tv_info);
        lvList = (ListView) findViewById(R.id.lv);
        btnSet = (Button) findViewById(R.id.tv_reset);
        etThread = (EditText) findViewById(R.id.et_thd);
        etDepth = (EditText) findViewById(R.id.et_depth);

        btnStart.setOnClickListener(this);
        btnStop.setOnClickListener(this);
        btnSet.setOnClickListener(this);
        lvList.setAdapter(mAdapter);

        thread = 1;
        depth = -1;//全盘扫描
        detail = true;//是否获取文件详情
        fileScanner = new FileScanner();
        suf = new String[]{};//查找所有文件
        //suf = new String[]{"mp3", "mp4", "avi", "rmvb"};
        //suf = new String[]{"jpg", "jpeg", "png", "bmp", "gif"};
        //suf = new String[]{"EBK2","EBK3","TXT","EPUB","CHM","UMD","PDF", "OPUB", "DOC", "DOCX",
        //        "WPS", "XLS", "XLSX", "ET", "PPT", "PPTX", "DPS"};
        //suf = new String[]{"jpg", "jpeg", "png", "bmp", "gif", "mp3", "mp4", "avi", "rmvb", "EBK2","EBK3","TXT","EPUB","CHM","UMD","PDF", "OPUB", "DOC", "DOCX",
        //                "WPS", "XLS", "XLSX", "ET", "PPT", "PPTX", "DPS"};
        fileScanner.initScanner(suf, thread, depth, detail);
        fileScanner.setScanCallback(callback);
        String sdPath = Environment.getExternalStorageDirectory().getAbsolutePath();
        //path = new String[] {sdPath+"/Android", sdPath+"DCIM"};
        path = new String[] {sdPath};
        fileScanner.startScan(path);

        etDepth.setText(""+depth);
        etThread.setText(""+thread);
    }

    @Override
    public void onClick(View view) {
        switch (view.getId()) {
            case R.id.tv_start:
                fileScanner.startScan(path);
                break;
            case R.id.tv_stop:
                if (fileScanner != null) fileScanner.stopScan();
                break;
            case R.id.tv_reset:
                String thd = etThread.getText().toString();
                thread = Integer.parseInt(TextUtils.isEmpty(thd) ? "1" : thd);
                String deep = etDepth.getText().toString();
                depth = Integer.parseInt(TextUtils.isEmpty(deep) ? "-1" : deep);
                fileScanner.initScanner(suf, thread, depth, detail);
                break;
        }
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        fileScanner.recycle();
    }

    private static class FindItem {
        String path;
        long size;
        long modifyTime;
    }

    private class MyAdapter extends BaseAdapter {

        private List<FindItem> mData;
        private SimpleDateFormat mFormat = new SimpleDateFormat("yyyy-MM-dd HH:mm");
        private Date mDate = new Date();

        public void changeData(List<FindItem> items) {
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
            FindItem item = mData.get(i);
            mDate.setTime(item.modifyTime*1000);
            tv.setText(item.path.substring(item.path.lastIndexOf('/')+1) + "   "+mFormat.format(mDate) + "   size:"+item.size/1024+"KB");
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
