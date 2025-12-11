# USB Driver - Pełna Struktura

## 📁 Struktura Katalogów i Plików

```
kernel/drivers/usb/
├── core/                          # USB Core Layer
│   ├── usb_core.c                 # Główna logika USB core
│   ├── usb_manager.c              # Zarządzanie kontrolerami i urządzeniami
│   ├── usb_device.c               # Zarządzanie urządzeniami USB
│   ├── usb_enumeration.c           # Proces enumeracji urządzeń
│   ├── usb_descriptors.c           # Parsowanie deskryptorów USB
│   ├── usb_transfer.c              # Zarządzanie transferami
│   └── usb_init.c                  # Inicjalizacja USB subsystem
│
├── host/                          # Host Controller Drivers
│   ├── xhci.c                      # XHCI (USB 3.0+) driver
│   ├── ehci.c                      # EHCI (USB 2.0) driver
│   ├── ehci_qh.c                   # EHCI Queue Head management
│   ├── ehci_td.c                   # EHCI Transfer Descriptor management
│   ├── ohci.c                      # OHCI (USB 1.1) driver
│   ├── uhci.c                      # UHCI (USB 1.1) driver
│   └── pci_usb_detect.c            # PCI detection dla kontrolerów USB
│
├── xhci/                          # XHCI Specific Modules
│   └── xhci_ring.c                 # TRB ring management (command/event/transfer)
│
├── hid/                           # USB HID Drivers
│   ├── hid_driver.cpp              # Główny sterownik HID (C++)
│   ├── hid_mouse.c                 # Integracja myszy USB HID
│   ├── hid_keyboard.c              # Integracja klawiatury USB HID
│   └── parser/                     # HID Report Descriptor Parser (C++)
│       ├── hid_parser.cpp          # High-level HID parser interface
│       └── hid_report_descriptor.cpp # Pełny parser HID report descriptor
│
└── util/                          # USB Utilities
    └── usb_helpers.c               # Funkcje pomocnicze (CRC, konwersje, etc.)

kernel/include/drivers/usb/
├── usb_core.h                      # Główne struktury i API USB core
├── usb_device.h                    # Struktury i funkcje urządzeń
├── usb_descriptors.h               # Struktury deskryptorów USB
├── usb_transfer.h                  # API transferów USB
├── usb_hid.h                       # API HID
├── xhci.h                          # Struktury i definicje XHCI
│
├── core/
│   └── usb.h                       # Core USB definitions
│
├── hid/                            # HID Headers (C++)
│   ├── hid_usage.hpp               # Usage pages i usage codes
│   ├── hid_field.hpp               # HID report field structures
│   ├── hid_report_descriptor.hpp   # HID report descriptor parser
│   └── hid_parser.hpp              # High-level HID parser interface
│
└── util/                           # Utility Headers
    ├── usb_log.h                   # Makra logowania USB
    ├── usb_bits.h                  # Makra manipulacji bitami
    └── usb_helpers.h               # Deklaracje funkcji pomocniczych
```

## 📋 Opis Modułów

### 🔵 Core Layer (`core/`)

**usb_core.c**
- Inicjalizacja i cleanup USB subsystem
- Zarządzanie listą kontrolerów host
- Zarządzanie listą urządzeń USB
- Alokacja adresów urządzeń

**usb_manager.c**
- Rejestracja/odrejestrowanie kontrolerów
- Wyszukiwanie kontrolerów po typie
- Debug printing

**usb_device.c**
- Alokacja/zwalnianie urządzeń USB
- Zarządzanie stanem urządzenia
- Zarządzanie endpointami
- Lista urządzeń

**usb_enumeration.c**
- Pełny proces enumeracji:
  1. Get device descriptor (address 0)
  2. Set address
  3. Get full device descriptor
  4. Get configuration descriptors
  5. Set configuration
- Dodawanie do listy urządzeń

**usb_descriptors.c**
- Parsowanie wszystkich typów deskryptorów:
  - Device descriptor
  - Configuration descriptor
  - Interface descriptor
  - Endpoint descriptor
  - HID descriptor
  - String descriptor
- Funkcje pobierania deskryptorów z urządzenia

**usb_transfer.c**
- Alokacja/zwalnianie transferów
- Submission i cancellation transferów
- Helper functions dla:
  - Control transfers
  - Interrupt transfers
  - Bulk transfers
  - Isochronous transfers
- Setup packet builder

**usb_init.c**
- Inicjalizacja całego USB stack
- Integracja z kernel
- Wywołanie inicjalizacji core i HID

### 🟢 Host Controllers (`host/`)

**xhci.c**
- XHCI controller initialization
- Reset controller i portów
- Transfer functions (placeholder - pełna implementacja wymaga TRB)
- PCI probe dla XHCI
- Cleanup

**ehci.c**
- EHCI controller initialization
- Reset controller i portów
- Transfer functions (placeholder)
- Cleanup

**ehci_qh.c**
- Queue Head allocation/freeing
- Queue Head initialization dla endpointów
- Linking queue heads
- Physical address management

**ehci_td.c**
- Transfer Descriptor allocation/freeing
- TD initialization
- Linking transfer descriptors
- Completion checking
- Length extraction

**ohci.c**
- OHCI controller initialization
- Reset controller i portów
- Transfer functions (placeholder)
- Cleanup

**uhci.c**
- UHCI controller initialization
- Reset controller i portów
- Transfer functions (placeholder)
- Cleanup

**pci_usb_detect.c**
- Skanowanie PCI bus dla kontrolerów USB
- Wykrywanie UHCI/OHCI/EHCI/XHCI
- Rejestracja kontrolerów w USB core
- Konfiguracja MMIO base addresses
- IRQ assignment

### 🟡 XHCI Specific (`xhci/`)

**xhci_ring.c**
- Command ring management:
  - Allocation i initialization
  - Enqueue TRBs
  - Physical address setup
- Event ring management:
  - Allocation i initialization
  - ERST (Event Ring Segment Table) setup
  - Dequeue event TRBs
- Transfer ring management:
  - Allocation per endpoint
  - Enqueue TRBs
  - Freeing
- TRB building functions:
  - Normal TRB
  - Link TRB
- Cycle bit management
- Memory barriers dla synchronizacji

### 🟣 HID Drivers (`hid/`)

**hid_driver.cpp** (C++)
- Główny sterownik HID
- Device probing
- Mouse initialization z HID parser
- Keyboard initialization z HID parser
- Read functions używające parsera:
  - `usb_hid_mouse_read()` - parsuje raporty myszy
  - `usb_hid_keyboard_read()` - parsuje raporty klawiatury
- Fallback do boot protocol

**hid_mouse.c**
- Integracja myszy USB HID z AstraOS
- Device scanning i registration
- Polling myszy
- Event pushing do GUI system
- Position tracking
- Screen bounds clamping

**hid_keyboard.c**
- Integracja klawiatury USB HID z AstraOS
- Device registration
- Polling klawiatury
- Scancode to ASCII conversion
- Modifier handling (Shift)
- Event pushing do GUI system

**parser/hid_parser.cpp** (C++)
- High-level HID parser interface
- Initialization z urządzenia USB
- Pobieranie HID descriptor
- Pobieranie report descriptor
- Parsowanie raportów:
  - `parse_mouse_report()` - automatyczne mapowanie X, Y, Wheel, Buttons
  - `parse_keyboard_report()` - parsowanie modyfikatorów i klawiszy

**parser/hid_report_descriptor.cpp** (C++)
- Pełny parser HID Report Descriptor
- Parsowanie short i long items
- Main items (Input, Output, Feature, Collection)
- Global items (Usage Page, Logical Min/Max, Report Size/Count, etc.)
- Local items (Usage, Usage Min/Max)
- Building report field structures
- Bit offset calculation
- Usage page/usage code mapping

### 🔴 Utilities (`util/`)

**usb_helpers.c**
- CRC calculations:
  - `usb_crc5()` - dla tokenów USB
  - `usb_crc16()` - dla danych USB
- String conversions:
  - `usb_speed_string()` - enum to string
  - `usb_controller_type_string()` - enum to string
  - `usb_endpoint_type_string()` - type to string
- Endpoint helpers:
  - `usb_endpoint_address()` - extract address
  - `usb_endpoint_direction()` - extract direction
  - `usb_endpoint_type()` - extract type
  - `usb_endpoint_sync_type()` - extract sync type
  - `usb_endpoint_usage_type()` - extract usage type
- Frame/microframe calculations:
  - `usb_frame_from_microframe()`
  - `usb_microframe_from_frame()`
- Utility functions:
  - `usb_wait_for_condition()` - timeout waiting
  - `usb_dump_descriptor()` - hex dump dla debug

**usb_log.h**
- Makra logowania:
  - `USB_LOG_ERROR`, `USB_LOG_WARN`, `USB_LOG_INFO`, `USB_LOG_DEBUG`
  - Controller-specific: `USB_LOG_XHCI`, `USB_LOG_EHCI`, `USB_LOG_OHCI`, `USB_LOG_UHCI`, `USB_LOG_HID`

**usb_bits.h**
- Bit manipulation macros:
  - `USB_BIT()`, `USB_BITS()` - bit masks
  - `USB_BIT_SET()`, `USB_BIT_CLEAR()`, `USB_BIT_TEST()` - operations
  - `USB_BIT_EXTRACT()`, `USB_BIT_INSERT()` - field extraction/insertion
  - `USB_MMIO_READ_BITS()`, `USB_MMIO_WRITE_BITS()` - MMIO operations

## 🔗 Zależności

```
USB Core
  ├── USB Host Controllers (UHCI/OHCI/EHCI/XHCI)
  │   └── PCI Detection
  ├── USB Device Management
  │   └── USB Enumeration
  ├── USB Transfer Management
  └── USB HID
      ├── HID Driver (C++)
      │   └── HID Parser (C++)
      │       └── HID Report Descriptor Parser (C++)
      ├── Mouse Integration
      └── Keyboard Integration
```

## 📊 Statystyki

- **Pliki C**: 18
- **Pliki C++**: 4
- **Pliki Header**: 15
- **Moduły Core**: 7
- **Host Controllers**: 6 (+ 2 EHCI helpers)
- **HID Drivers**: 3 (+ 2 parser modules)
- **Utilities**: 1 (+ 3 headers)

## 🎯 Funkcjonalność

✅ **Zaimplementowane:**
- USB Core Layer (device management, enumeration, transfers)
- XHCI driver (basic structure + TRB rings)
- EHCI helpers (QH/TD management)
- USB HID stack (mouse + keyboard)
- HID Report Descriptor Parser (pełny parser w C++)
- PCI detection dla kontrolerów USB
- Integration z AstraOS event system

🚧 **W trakcie implementacji:**
- Pełna implementacja transferów XHCI (TRB-based)
- UHCI/OHCI/EHCI drivers (basic structure ready)
- Hotplug support

📝 **TODO:**
- Pełna implementacja UHCI/OHCI/EHCI
- MSI/MSI-X support dla XHCI
- USB hub support
- Isochronous transfers
- USB 3.0+ features (SuperSpeed)

