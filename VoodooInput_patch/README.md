# Patch per VoodooInput 1.1.7

`VoodooInput-1.1.7-XPS9560.patch` modifica [acidanthera/VoodooInput](https://github.com/acidanthera/VoodooInput)
al commit `d897813` ("Bump version", versione 1.1.7). Serve alla release 1.1 di questo fork.

File modificati:

- `VoodooInput/VoodooInput.cpp`
  - **Rev 14:** se un genitore ha `Transport = USB` non crea il `TrackpointDevice`. Log all'avvio:
    `VoodooInput REV 14: transport=USB, trackpoint NOT created`;
  - **Rev 10:** `stop()` fa stop/detach a mano solo dei figli che non sono già in terminazione.
- `VoodooInput/Trackpoint/TrackpointDevice.cpp/.hpp`
  - **Rev 12:** durante la terminazione trattiene i propri figli, poi li rilascia in `free()`.

Su touchpad I2C e PS2 il comportamento resta quello originale, a parte le correzioni di sicurezza di `stop()`.

## Compilazione

```bash
git clone https://github.com/acidanthera/VoodooInput.git
cd VoodooInput
git checkout d897813
git apply /percorso/di/VoodooInput-1.1.7-XPS9560.patch
git clone https://github.com/acidanthera/MacKernelSDK.git
xcodebuild -project VoodooInput.xcodeproj -target VoodooInput -configuration Release CODE_SIGNING_ALLOWED=NO SYMROOT="$PWD/build"
```

Il kext si trova in `build/Release/VoodooInput.kext`. Va messo al posto del VoodooInput abilitato nel config.plist
(di solito `VoodooPS2Controller.kext/Contents/PlugIns/VoodooInput.kext`).
