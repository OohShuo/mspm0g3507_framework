# External Image Assets

Large images can be stored in the W25Q32 LittleFS partition and streamed into
the LCD one scanline at a time. The MCU does not decode JPG or PNG. The PC
converts source artwork into the native `R565` format before upload.

## R565 format

- 16-byte header
- RGB565 pixels in row-major, little-endian order
- Optional row-aligned 1-bit alpha mask
- A 240x320 opaque image uses 153,616 bytes

The runtime API is in `src/app/image_asset/image_asset.h`.

The W25Q32 has 4 MiB total capacity. The current LittleFS partition uses the
upper 2 MiB (`0x200000` through `0x3fffff`), which fits about thirteen opaque
240x320 images. Expanding LittleFS to the full chip would require auditing the
reserved lower half and reformatting the filesystem.

## Upload workflow

The UART file manager and the game console use separate firmware profiles to
leave enough runtime heap for games.

1. In `config/app_config.h`, temporarily set:

   ```c
   #define FLASH_MGR_ENABLE 1
   #define GAME_CONSOLE_ENABLE 0
   ```

2. Build and flash the upload firmware:

   ```powershell
   python scripts/cm.py
   ```

3. Convert and upload an image:

   ```powershell
   python scripts/flash_manager.py COM3 upload-image assets/images/bg2.jpg /air_bg.r565 --width 240 --height 320 --fit cover
   ```

   Install host dependencies when needed:

   ```powershell
   pip install pyserial pillow
   ```

4. Restore the normal game configuration:

   ```c
   #define FLASH_MGR_ENABLE 0
   #define GAME_CONSOLE_ENABLE 1
   ```

5. Build and flash the game firmware. Reflashing internal MCU Flash does not
   erase files stored in the external W25Q32.

## Air Battle

Air Battle automatically tries `/air_bg.r565` as a 240x320 background. If the
file is missing, corrupt, or has a different size, the built-in low-resolution
background remains available as a fallback.

For transparent assets, add `--mask` and use `--fit contain`. The file format
already stores the mask, while sprite compositing can be migrated incrementally
without changing the uploaded files.
