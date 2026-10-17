# VoodooI2CHID per Dell XPS 15 9560 (macOS Tahoe)

Fork di [VoodooI2CHID](https://github.com/VoodooI2C/VoodooI2CHID), il satellite HID di
[VoodooI2C](https://github.com/VoodooI2C/VoodooI2C) 2.9.1, corretto per il **touchscreen del Dell XPS 15 9560**
su un Hackintosh con **macOS Tahoe 26** (testato su 26.7.1, build 25G241).

Problema risolto: con il touchscreen abilitato il Mac **non si spegneva**. Allo spegnimento andava in kernel panic
oppure si bloccava.

## Stato attuale: versione 1.0 (Rev 9) ✅

| Funzione | Stato |
|---|---|
| Tocco con un dito (clic) | ✅ |
| Trascinamento / selezione | ✅ |
| Clic destro con pressione prolungata | ✅ |
| Spegnimento e riavvio puliti | ✅ |
| Scroll a due dita, zoom, rotazione, gesti | ❌ disattivati sul touchscreen (vedi sotto) |
| Touchpad I2C (gesti compresi) | ✅ invariato |

Il kext pronto è nella [release 1.0](../../releases/tag/v1.0).

## Configurazione di prova

- Dell XPS 15 9560, OpenCore, SMBIOS MacBookPro16,4
- Touchscreen collegato via **USB**, gestito dal driver Apple DriverKit `AppleUserUSBHostHIDDevice`
- Touchpad **I2C** (TPD1), tastiera PS2
- VoodooI2C 2.9.1, VoodooPS2Controller 2.3.8 (con plugin VoodooInput 1.1.7)
- boot-args usati per la diagnosi: `-v debug=0x100 keepsyms=1 -vi2c-force-polling`

## Il problema

Allo spegnimento si presentava uno di questi due casi:

- **kernel panic** in `IOHIDDevice::didTerminate + 0x1e` (`CR2 = 0x48`, `RAX = 0`, thread
  `terminateThread` → `actionDidTerminate`);
- oppure un **blocco** subito dopo
  `DK: IOUserServer(com.apple.driverkit.AppleUserHIDDrivers)::systemHalt()` / `terminate(AppleUserUSBHostHIDDevice…)`.

## Diagnosi

- Su Tahoe `IOHIDDevice::didTerminate` esegue `_reserved->eventSource->lock()`. `RAX = 0` / `CR2 = 0x48` indica che
  `_reserved` è già NULL, cioè che l'oggetto è già stato liberato (**use-after-free**).
- Dall'albero `ioreg` si vede che il VoodooI2CHID, sotto il touchscreen **USB**, costruisce un intero sottoalbero:

  ```
  AppleUserUSBHostHIDDevice → IOHIDInterface → VoodooI2CTouchscreenHIDEventDriver
    → VoodooI2CMultitouchInterface → VoodooI2CNativeEngine → VoodooInput
      → VoodooInputSimulatorDevice / VoodooInputActuatorDevice / TrackpointDevice (tre IOHIDDevice)
  ```

  Allo spegnimento DriverKit termina il dispositivo USB e con lui tutto questo sottoalbero (il ramo I2C del touchpad
  invece non viene terminato).
- Nell'output del panic, le ultime righe prima del crash sono sempre
  `VoodooInputActuatorDevice … close` e `VoodooInputSimulatorDevice … close by AppleMultitouchTrackpadHIDEventDriver`.
  Il colpevole è quindi il sottoalbero **VoodooInput sotto il dispositivo USB**.
- Togliendo quel sottoalbero (Rev 9) lo spegnimento torna pulito: conferma definitiva.

## Modifiche rispetto all'upstream

`VoodooI2CHIDDevice.cpp/.hpp`
- override di `didTerminate`, che chiama direttamente `IOService::didTerminate` (Rev 8).

`VoodooI2CMultitouchHIDEventDriver.cpp`
- gestione del power management solo per i trasporti non USB (Rev 8);
- **Rev 9:** sul trasporto USB **non** viene creata la `VoodooI2CMultitouchInterface`, quindi niente VoodooInput sotto
  il touchscreen. All'avvio il log mostra `REV 9: transport=USB, multitouch interface NOT created`;
- **Rev 9:** in `handleStop` l'interfaccia multitouch viene fermata e staccata a mano solo se non è già in
  terminazione (`isInactive()`), per non smontare due volte il sottoalbero.

`VoodooI2CTouchscreenHIDEventDriver.cpp`
- **Rev 9:** `handleStop` rimuove e cancella tutti e 3 i timer, mentre prima ne rimuoveva solo 1.

`Info.plist`
- rimossa la personality "VoodooI2CHIDDevice Generic Mouse HID Event Driver".

## Cronologia delle revisioni

| Revisione | Risultato |
|---|---|
| upstream / Rev ≤ 7 | panic `IOHIDDevice::didTerminate` allo spegnimento |
| Rev 8 (override `didTerminate`, PM solo non-USB) | a volte blocco, a volte ancora panic |
| **Rev 9** (niente VoodooInput sotto USB, stop sicuro, 3 timer) | ✅ **spegnimento pulito**, touchscreen funzionante senza gesti |
| Rev 10 (sperimentale: interfaccia di nuovo su USB + VoodooInput con `stop()` corretto) | gesti ok ma panic identico: la correzione di `VoodooInput::stop` non basta |

## Installazione

1. Scarica `VoodooI2CHID-1.0-Rev9.zip` dalla [release 1.0](../../releases/tag/v1.0).
2. Sostituisci `EFI/OC/Kexts/VoodooI2CHID.kext` con quello dello zip (VoodooI2C.kext 2.9.1 resta quello ufficiale).
3. Riavvia e controlla che sia caricato:
   ```bash
   kmutil showloaded --list-only | grep VoodooI2CHID
   ```
   UUID atteso: `905F4174-C94C-3999-83A0-834208E32532`.

## Compilazione

Il workflow GitHub Actions (`Actions → Compila VoodooI2CHID XPS 9560 → Run workflow`) clona VoodooI2C 2.9.1,
inserisce questi sorgenti al posto del satellite VoodooI2CHID e compila tutti i kext.

## Prossimi passi

- Recuperare scroll a due dita e gesti sul touchscreen USB senza il panic: va capito perché i figli IOHIDDevice di
  VoodooInput vengono liberati prima del loro `didTerminate` (servono `IOLog` su stop/willTerminate/didTerminate/free).

## Crediti

VoodooI2C e VoodooI2CHID: Alexandre Daoud e i contributori di VoodooI2C. VoodooInput: Kishor Prins, Leonard Kleinhans
e acidanthera.
