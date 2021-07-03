package d.hl.filescan;

import android.content.SharedPreferences;
import android.os.Bundle;
import android.os.Environment;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.BaseAdapter;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.ImageView;
import android.widget.ListView;
import android.widget.TextView;

import androidx.appcompat.app.AppCompatActivity;

import com.bumptech.glide.Glide;
import com.dhl.filescanner.AbstractScanCallback;
import com.dhl.filescanner.FileScanner;

import java.text.SimpleDateFormat;
import java.util.Collections;
import java.util.Comparator;
import java.util.Date;
import java.util.List;

public class ScannerActivity extends AppCompatActivity implements View.OnClickListener {

  private TextView mTVINfo;
  private EditText mETThread, mETDepth;
  private CheckBox mCBDetail, mCBHideDir, mCBNoMedia;

  private MyAdapter mAdapter = new MyAdapter();
  private FileScanner mFileScanner;
  private long mStartTime;
  private SharedPreferences sp;

  @Override
  protected void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);
    setContentView(R.layout.activity_scanner);

    Button btnStart = (Button) findViewById(R.id.tv_start);
    Button btnStop = (Button) findViewById(R.id.tv_stop);
    mTVINfo = (TextView) findViewById(R.id.tv_info);
    ListView lvList = (ListView) findViewById(R.id.lv);
    mETThread = (EditText) findViewById(R.id.et_thd);
    mETDepth = (EditText) findViewById(R.id.et_depth);
    mCBDetail = (CheckBox) findViewById(R.id.cb_detail);
    mCBHideDir = (CheckBox) findViewById(R.id.cb_hiddir);
    mCBNoMedia = (CheckBox) findViewById(R.id.cb_nomedia);

    btnStart.setOnClickListener(this);
    btnStop.setOnClickListener(this);
    lvList.setAdapter(mAdapter);

    sp = getSharedPreferences("prefs", MODE_PRIVATE);

    mETDepth.setText(sp.getString("depth", "-1"));
    mETThread.setText("4");
    mCBDetail.setChecked(true);
    mCBHideDir.setChecked(true);
    mCBNoMedia.setChecked(true);

  }

  @Override
  public void onClick(View view) {
    switch (view.getId()) {
      case R.id.tv_start:
        startScan();
        break;
      case R.id.tv_stop:
        if (mFileScanner != null) mFileScanner.stopScan();
        break;
    }
  }

  private void startScan() {
    if (mFileScanner != null) mFileScanner.stopScan();
    mFileScanner = new FileScanner();

    String[] suffixes = new String[]{};//查找所有文件
    suffixes = new String[]{"jpg", "jpeg", "png", "bmp", "gif", "DOC", "DOCX", "WPS", "XLS", "XLSX", "ET", "PPT", "PPTX"};

    String[] filteredsuffixes = null;
    if (mCBNoMedia.isChecked()) {
      filteredsuffixes = new String[]{"jpg", "jpeg", "png", "bmp", "gif"};
    }

    String thd = mETThread.getText().toString();
    int threadCount = Integer.parseInt(TextUtils.isEmpty(thd) ? "1" : thd);

    String deep = mETDepth.getText().toString();
    sp.edit().putString("depth", deep).apply();
    int scanDepth = Integer.parseInt(TextUtils.isEmpty(deep) ? "-1" : deep);

    String sdPath = Environment.getExternalStorageDirectory().getAbsolutePath();
    String[] scanPath = new String[]{sdPath};
    //scanPath = new String[] {sdPath+"/Android/", sdPath+"/DCIM/"};

    mFileScanner.setScanParams(suffixes, filteredsuffixes, threadCount, scanDepth, mCBDetail.isChecked());
    mFileScanner.setScanHiddenEnable(mCBHideDir.isChecked());
    mFileScanner.setScanPath(scanPath);


    mFileScanner.startScan(new AbstractScanCallback() {
      @Override
      public void onScanStart() {
        mStartTime = System.currentTimeMillis();
        runOnUiThread(new Runnable() {
          @Override
          public void run() {
            mTVINfo.setText("扫描中...");
            mAdapter.changeData(null);
          }
        });
      }

      @Override
      public void onScanFinish(final List<FileScanner.FindItem> files, final boolean isCancel) {
        final long time = System.currentTimeMillis() - mStartTime;
        final int count = files == null ? 0 : files.size();

        runOnUiThread(new Runnable() {
          @Override
          public void run() {
            String text = "files:" + count + "    time:" + time;
            mTVINfo.setText(text);

            if (files != null && files.size() > 0) {
              Collections.sort(files, new Comparator<FileScanner.FindItem>() {
                @Override
                public int compare(FileScanner.FindItem o1, FileScanner.FindItem o2) {
                  return (int) (o2.modifyTime - o1.modifyTime);
                }
              });
            }
            mAdapter.changeData(files);
          }
        });
      }
    });
  }

  private class MyAdapter extends BaseAdapter {

    private List<FileScanner.FindItem> mData;
    private SimpleDateFormat mFormat = new SimpleDateFormat("yyyy-MM-dd");
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
      if (view == null) {
        view = LayoutInflater.from(getApplicationContext()).inflate(R.layout.scan_item, viewGroup, false);
      }

      TextView tv = view.findViewById(R.id.tvName);
      ImageView iv = view.findViewById(R.id.ivImg);




      FileScanner.FindItem item = mData.get(i);
      if (mCBDetail.isChecked()) {
        StringBuilder sb = new StringBuilder();
        sb.append(item.path);
        sb.append("\n");
        mDate.setTime(item.modifyTime * 1000);
        String md = mFormat.format(mDate);
        sb.append(md);
        sb.append("    ");
        sb.append("size:");
        sb.append(String.format("%.2f", item.size / 1024f));
        sb.append("kb");
        tv.setText(sb.toString());
      } else {
        tv.setText(item.path.substring(item.path.lastIndexOf('/') + 1));
      }

      Glide.with(iv).load(item.path).into(iv);
      return view;
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
