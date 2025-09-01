# third_party

This directory is intentionally kept out of version control for large vendor assets.

Do NOT commit unpacked CEF distributions or archives here. Instead:

1. Download the CEF binary version pinned in `cmake/DownloadCEF.cmake`.
2. Extract into a directory like:
   `third_party/cef_binary_${CEF_VERSION}_${CEF_PLATFORM}`
3. Reconfigure/build the project.

This README is kept to ensure the folder exists in the repo without vendor blobs.

