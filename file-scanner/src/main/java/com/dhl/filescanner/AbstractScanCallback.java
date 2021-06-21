package com.dhl.filescanner;

import java.util.HashMap;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;

/**
 * Created by DuanHl on 2017/9/28.
 */

public abstract class AbstractScanCallback implements FileScanner.ScanCallback {

  private Map<Long, List<FileScanner.FindItem>> mFiles = new HashMap<>();

  @Override
  public void onStart() {
    mFiles.clear();
    onScanStart();
  }

  @Override
  public void onFind(long threadId, String path, long size, long modify) {
    List<FileScanner.FindItem> list = mFiles.get(threadId);
    if (list == null) {
      list = new LinkedList<>();
      synchronized (AbstractScanCallback.class) {
        mFiles.put(threadId, list);
      }
    }
    list.add(new FileScanner.FindItem(path, size, modify));
  }

  @Override
  public void onFinish(boolean isCancel) {
    List<FileScanner.FindItem> items = null;
    for (List<FileScanner.FindItem> list : mFiles.values()) {
      if (items == null) {
        items = list;
      } else {
        items.addAll(list);
      }
    }
    mFiles.clear();
    onScanFinish(items, isCancel);
  }

  public abstract void onScanStart();

  public abstract void onScanFinish(List<FileScanner.FindItem> files, boolean isCancel);
}
