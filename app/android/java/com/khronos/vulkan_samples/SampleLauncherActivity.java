/* Copyright (c) 2019-2024, Arm Limited and Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 the "License";
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.khronos.vulkan_samples;

import android.Manifest;
import android.annotation.SuppressLint;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;

import android.util.Log;
import android.view.Menu;
import android.view.MenuItem;
import android.widget.Toast;

import java.io.File;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.regex.Pattern;

import androidx.appcompat.widget.Toolbar;

import com.khronos.vulkan_samples.common.Notifications;
import com.khronos.vulkan_samples.model.Sample;
import com.khronos.vulkan_samples.model.SampleStore;
import com.khronos.vulkan_samples.views.SampleListView;

public class SampleLauncherActivity extends AppCompatActivity {

    SampleListView sampleListView;

    private boolean isBenchmarkMode = false;
    private boolean isHeadless = false;

    public SampleStore samples;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        Toolbar toolbar = findViewById(R.id.toolbar);
        setSupportActionBar(toolbar);

        if (loadNativeLibrary(getResources().getString(R.string.native_lib_name))) {
            // Get sample info from cpp cmake generated file
            samples = new SampleStore(Arrays.asList(getSamples()));
        }

        parseArguments();
        showSamples();
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        super.onCreateOptionsMenu(menu);
        getMenuInflater().inflate(R.menu.main_menu, menu);
        return true;
    }

    @Override
    public boolean onPrepareOptionsMenu(Menu menu) {
        super.onPrepareOptionsMenu(menu);
        MenuItem checkable = menu.findItem(R.id.menu_benchmark_mode);
        checkable.setChecked(isBenchmarkMode);

        checkable = menu.findItem(R.id.menu_headless);
        checkable.setChecked(isHeadless);
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        if(item.getItemId() == R.id.filter_button) {
            sampleListView.dialog.show(getSupportFragmentManager(), "filter");
            return true;
        } else if(item.getItemId() == R.id.menu_run_samples) {
            String category = "";
            ViewPagerAdapter adapter = ((ViewPagerAdapter) sampleListView.viewPager.getAdapter());
            if (adapter != null) {
                category = adapter.getCurrentFragment().getCategory();
            }

            List<String> arguments = new ArrayList<>();
            arguments.add("batch");
            arguments.add("--category");
            arguments.add(category);
            arguments.add("--tag");
            arguments.addAll(sampleListView.dialog.getFilter());

            String[] sa = {};
            launchWithCommandArguments(arguments.toArray(sa));
            return true;
        } else if(item.getItemId() == R.id.menu_benchmark_mode) {
            isBenchmarkMode = !item.isChecked();
            item.setChecked(isBenchmarkMode);
            return true;
        } else if(item.getItemId() == R.id.menu_headless) {
            isHeadless = !item.isChecked();
            item.setChecked(isHeadless);
            return true;
        } else {
            return super.onOptionsItemSelected(item);
        }
    }

    /**
     * Load a native library
     *
     * @param native_lib_name Native library to load
     * @return True if loaded correctly; False otherwise
     */
    private boolean loadNativeLibrary(String native_lib_name) {
        boolean status = true;

        try {
            System.loadLibrary(native_lib_name);
        } catch (UnsatisfiedLinkError e) {
            Toast.makeText(getApplicationContext(), "Native code library failed to load.", Toast.LENGTH_SHORT).show();

            status = false;
        }

        return status;
    }



    /**
     * Show/Create the Sample View. Hides all other views
     */
    public void showSamples() {
        if (sampleListView == null) {
            sampleListView = new SampleListView(this);
        }

        sampleListView.show();
    }

    // Allow Orientation locking for testing
    @SuppressLint("SourceLockedOrientationActivity")
    public void parseArguments() {
        // Handle argument passing
        Bundle extras = this.getIntent().getExtras();
        if (extras != null) {
            if (extras.containsKey("cmd")) {
                String[] commands = null;
                String[] command_arguments = extras.getStringArray("cmd");
                String command = extras.getString("cmd");
                if (command_arguments != null) {
                    commands = command_arguments;
                } else if (command != null) {
                    commands = command.split("[ ]+");
                }

                if (commands != null) {
                    for (String cmd : commands) {
                        if (cmd.equals("test")) {
                            setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
                            break;
                        }
                    }
                    launchWithCommandArguments(commands);
                    finishAffinity();
                }

            } else if (extras.containsKey("sample")) {
                // launchSample(extras.getString("sample"));
                launchSampleWithExternalSurface(extras.getString("sample"));
                finishAffinity();
            } else if (extras.containsKey("test")) {
                launchTest(extras.getString("test"));
                finishAffinity();
            }

        }
    }

    /**
     * Set arguments of the Native Activity You may also use a String[]
     *
     * @param args A string array of arguments
     */
    public void setArguments(String... args) {
        List<String> arguments = new ArrayList<>(Arrays.asList(args));
        if (isBenchmarkMode) {
            arguments.add("--benchmark");
        }
        if (isHeadless) {
            arguments.add("--headless_surface");
        }
        String[] argArray = new String[arguments.size()];
        sendArgumentsToPlatform(arguments.toArray(argArray));
    }

    public void launchWithCommandArguments(String... args) {
        setArguments(args);
        // Use SurfaceSampleActivity for better UI experience
        Intent intent = new Intent(SampleLauncherActivity.this, SurfaceSampleActivity.class);
        intent.putExtra("sample_args", args);
        startActivity(intent);
    }

    /**
     * Launch with command arguments using traditional NativeActivity mode
     * This provides the original approach for compatibility
     *
     * @param args Command line arguments
     */
    public void launchWithCommandArgumentsNative(String... args) {
        setArguments(args);
        Intent intent = new Intent(SampleLauncherActivity.this, NativeSampleActivity.class);
        startActivity(intent);
    }

    public void launchTest(String testID) {
        setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        launchWithCommandArguments("test", testID);
    }

    public void launchSample(String sampleID) {
        Sample sample = samples.findByID(sampleID);
        if (sample == null) {
            Notifications.toast(this, "Could not find sample " + sampleID);
            showSamples();
            return;
        }
        // Use the new external surface mode for better UI
        launchSampleWithExternalSurface(sampleID);
    }

    /**
     * Launch a sample using traditional NativeActivity mode
     * This provides the original GameActivity approach for compatibility
     *
     * @param sampleID The ID of the sample to launch
     */
    public void launchSampleWithNativeActivity(String sampleID) {
        Sample sample = samples.findByID(sampleID);
        if (sample == null) {
            Notifications.toast(this, "Could not find sample " + sampleID);
            showSamples();
            return;
        }
        
        // Use traditional approach with setArguments and NativeSampleActivity
        List<String> arguments = new ArrayList<>();
        arguments.add("sample");
        arguments.add(sampleID);
        
        if (isBenchmarkMode) {
            arguments.add("--benchmark");
        }
        if (isHeadless) {
            arguments.add("--headless_surface");
        }
        
        // Set arguments for native platform
        String[] argArray = new String[arguments.size()];
        sendArgumentsToPlatform(arguments.toArray(argArray));
        
        // Launch traditional NativeActivity
        Intent intent = new Intent(SampleLauncherActivity.this, NativeSampleActivity.class);
        startActivity(intent);
    }

    /**
     * Launch a sample using external surface mode (SurfaceSampleActivity)
     * This provides an alternative to the traditional GameActivity approach
     *
     * @param sampleID The ID of the sample to launch
     */
    public void launchSampleWithExternalSurface(String sampleID) {
        Sample sample = samples.findByID(sampleID);
        if (sample == null) {
            Notifications.toast(this, "Could not find sample " + sampleID);
            showSamples();
            return;
        }

        // Prepare arguments for the external surface mode
        List<String> arguments = new ArrayList<>();
        arguments.add("sample");
        arguments.add(sampleID);
        
        if (isBenchmarkMode) {
            arguments.add("--benchmark");
        }
        if (isHeadless) {
            arguments.add("--headless_surface");
        }

        setArguments(arguments.toArray(new String[0]));

        // Launch the external surface activity
        Intent intent = new Intent(SampleLauncherActivity.this, SurfaceSampleActivity.class);
        intent.putExtra("sample_args", arguments.toArray(new String[0]));
        startActivity(intent);
    }

    /**
     * Get samples from the Native Application
     *
     * @return A list of Samples that are currently installed in the Native
     *         Application
     */
    private native Sample[] getSamples();

    /**
     * Set the arguments of the Native Application
     *
     * @param args The arguments that are to be passed to the app
     */
    private native void sendArgumentsToPlatform(String[] args);
}
