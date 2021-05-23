package d.hl.filescan;

import android.Manifest;
import android.content.Intent;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.provider.Settings;
import android.view.View;
import android.widget.Toast;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;
import kotlin.Unit;
import kotlin.jvm.functions.Function0;

public class MainActivity extends AppCompatActivity {


  @Override
  protected void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);

    setContentView(R.layout.activity_main);

    findViewById(R.id.tv_start).setOnClickListener(new View.OnClickListener() {
      @Override
      public void onClick(View v) {
        if (Build.VERSION.SDK_INT <= Build.VERSION_CODES.Q) {
          PermissionHelper.Companion.with(MainActivity.this)
                  .permission(Manifest.permission.READ_EXTERNAL_STORAGE,
                          Manifest.permission.WRITE_EXTERNAL_STORAGE)
                  .onGranted(new Function0<Unit>() {
                    @Override
                    public Unit invoke() {
                      startActivity(new Intent(getApplicationContext(), ScannerActivity.class));
                      return null;
                    }
                  })
                  .start();
        } else {
          PermissionHelper.Companion.with(MainActivity.this)
                  .intentOrNull(new Function0<Intent>() {
                    @Override
                    public Intent invoke() {
                      if (!Environment.isExternalStorageManager()) {
                        return new Intent(Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION);
                      } else {
                        return null;
                      }
                    }
                  })
                  .onIntentResult(new Function0<Unit>() {
                    @Override
                    public Unit invoke() {
                      if (Environment.isExternalStorageManager()) {
                        startActivity(new Intent(getApplicationContext(), ScannerActivity.class));
                      }
                      return null;
                    }
                  })
                  .start();
        }

      }
    });
  }

  @Override
  protected void onActivityResult(int requestCode, int resultCode, @Nullable Intent data) {
    super.onActivityResult(requestCode, resultCode, data);

    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
      if (requestCode == 100) {
        String text = "permission fail";
        if (Environment.isExternalStorageManager()) {
          text = "permission ok";
        }
        Toast.makeText(this, text, Toast.LENGTH_SHORT).show();
      }
    }
  }
}
