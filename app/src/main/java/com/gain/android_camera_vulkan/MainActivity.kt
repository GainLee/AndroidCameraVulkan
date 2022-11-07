package com.gain.android_camera_vulkan

import android.os.Bundle
import androidx.appcompat.app.AppCompatActivity
import com.gain.android_camera_vulkan.databinding.ActivityMainBinding

class MainActivity : AppCompatActivity() {

    private lateinit var binding: ActivityMainBinding

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        supportFragmentManager.beginTransaction()
            .replace(
                R.id.fragmentContainer,
                CameraFragment.newInstance()
            )
            //                .addToBackStack("Choose")
            .commit()
    }
}