package com.intel.realsense.camera;

import android.Manifest;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.support.v4.app.ActivityCompat;
import android.support.v4.content.ContextCompat;
import android.support.v7.app.AppCompatActivity;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;

import com.intel.realsense.librealsense.CameraInfo;
import com.intel.realsense.librealsense.Device;
import com.intel.realsense.librealsense.DeviceList;
import com.intel.realsense.librealsense.DeviceListener;
import com.intel.realsense.librealsense.UpdateDevice;
import com.intel.realsense.librealsense.ProductClass;
import com.intel.realsense.librealsense.RsContext;

public class DetachedActivity extends AppCompatActivity {
    private static final String TAG = "librs camera detached";
    private static final int PERMISSIONS_REQUEST_CAMERA = 0;
    private static final int PLAYBACK_REQUEST_CODE = 1;

    private boolean mPermissionsGrunted = false;
    private Button mPlaybackButton;

    private Context mAppContext;
    private RsContext mRsContext = new RsContext();

    private boolean mFinished = false;
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_detached);

        mAppContext = getApplicationContext();

        mPlaybackButton = findViewById(R.id.playbackButton);
        mPlaybackButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                Intent intent = new Intent(mAppContext, PlaybackActivity.class);
                startActivityForResult(intent, PLAYBACK_REQUEST_CODE);
            }
        });

        if (android.os.Build.VERSION.SDK_INT > android.os.Build.VERSION_CODES.O &&
                ContextCompat.checkSelfPermission(this, Manifest.permission.CAMERA) != PackageManager.PERMISSION_GRANTED) {
            ActivityCompat.requestPermissions(this, new String[]{Manifest.permission.CAMERA}, PERMISSIONS_REQUEST_CAMERA);
            return;
        }

        String appVersion = BuildConfig.VERSION_NAME;
        String lrsVersion = RsContext.getVersion();
        TextView versions = findViewById(R.id.versionsText);
        versions.setText("librealsense version: " + lrsVersion + "\ncamera app version: " + appVersion);

        mFinished = false;
        mPermissionsGrunted = true;
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, String permissions[], int[] grantResults) {
        if (ContextCompat.checkSelfPermission(this, Manifest.permission.CAMERA) != PackageManager.PERMISSION_GRANTED) {
            ActivityCompat.requestPermissions(this, new String[]{Manifest.permission.CAMERA}, PERMISSIONS_REQUEST_CAMERA);
            return;
        }

        mPermissionsGrunted = true;
    }

    @Override
    protected void onResume() {
        super.onResume();

        if(mPermissionsGrunted)
            init();
    }

    private void init() {
        RsContext.init(getApplicationContext());
        mRsContext.setDevicesChangedCallback(mListener);
        validatedDevice();
    }

    private synchronized void validatedDevice(){
        if(mFinished)
            return;
        try(DeviceList dl = mRsContext.queryDevices()){
            if(dl.getDeviceCount() == 0)
                return;
            try(Device device = dl.createDevice(0)){
                if(device instanceof UpdateDevice){
                    mFinished = true;
                    Intent intent = new Intent(mAppContext, FirmwareUpdateActivity.class);
                    startActivity(intent);
                    finish();
                }
                else {
                    if (!verifyMinimalFwVersion(device))
                        return;
                    mFinished = true;
                    Intent intent = new Intent(mAppContext, PreviewActivity.class);
                    startActivity(intent);
                    finish();
                }
            }
        }
    }

    private boolean verifyMinimalFwVersion(Device device){
        if (!device.supportsInfo(CameraInfo.RECOMMENDED_FIRMWARE_VERSION))
            return true;
        final String recFw = device.getInfo(CameraInfo.RECOMMENDED_FIRMWARE_VERSION);
        final String fw = device.getInfo(CameraInfo.FIRMWARE_VERSION);
        String[] sFw = fw.split("\\.");
        String[] sRecFw = recFw.split("\\.");
        for (int i = 0; i < sRecFw.length; i++) {
            if (Integer.parseInt(sFw[i]) > Integer.parseInt(sRecFw[i]))
                break;
            if (Integer.parseInt(sFw[i]) < Integer.parseInt(sRecFw[i])) {
                mFinished = true;
                Intent intent = new Intent(mAppContext, FirmwareUpdateActivity.class);
                startActivity(intent);
                finish();
                return false;
            }
        }
        return true;
    }

    private DeviceListener mListener = new DeviceListener() {
        @Override
        public void onDeviceAttach() {
            validatedDevice();
        }

        @Override
        public void onDeviceDetach() {
            Intent intent = new Intent(mAppContext, DetachedActivity.class);
            startActivity(intent);
            finish();
        }
    };
}
