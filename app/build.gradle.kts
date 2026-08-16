plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

android {
    namespace = "com.kivapfa.player"
    compileSdk = 35
    defaultConfig {
        applicationId = "com.kivapfa.player"
        minSdk = 26
        targetSdk = 35
        versionCode = 1
        versionName = "0.1.0"
        externalNativeBuild { cmake { cppFlags += "-std=c++20 -O3 -ffast-math" } }
        ndk { abiFilters += listOf("arm64-v8a", "x86_64") }
    }
    externalNativeBuild { cmake { path = file("src/main/cpp/CMakeLists.txt"); version = "3.22.1" } }
    buildFeatures { viewBinding = false }
}
