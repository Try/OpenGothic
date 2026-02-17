# Add project specific ProGuard rules here.
# By default, the flags in this file are appended to flags specified
# in the Android SDK.

# Keep native methods
-keepclasseswithmembernames class * {
    native <methods>;
}
