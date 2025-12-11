# AstraOS Development TODO (Post–0.0.1.minor Roadmap)
Author: v3nn7  
Project: AstraOS

## 1. USB / XHCI Bring-Up (High Priority)
You have:
- xHCI structures  
- TRB/ring stubs  
- USB/HID scaffolding  
- PCI BAR MMIO mapping  
- Fake-host HID tests working  

You still need:
- Complete xHCI Command Ring implementation  
- Event Ring + ERST logic  
- Port management  
- Real USB device enumeration  
- Control transfer pipeline  
- HID interrupt endpoints  

## 2. MSI / MSI-X System Completion
- MSI allocator  
- MSI-X table mapping  
- IOAPIC fallback  
- PCI integration  

## 3. AHCI Implementation
- HBA init  
- Port bring-up  
- DMA read implementation  
- Raw LBA loader  

## 4. Memory System Enhancements
- HHDM fallback handling  
- Page table debug dump  
- DMA-safe allocator  
- Per-CPU memory structures  

## 5. SMP Bring-Up
- AP trampoline  
- Per-CPU LAPIC  
- CPU enumeration  
- Per-CPU data  

## 6. Kernel Runtime Layer
- Unified kprintf (serial + fb)  
- Panic handler improvements  
- Kernel heap  
- Logging levels  

## 7. Core Driver Completion
### USB:
- Pipe abstraction  
- GetStringDescriptor  
- EP0 fallback  

### PCI:
- Extended capabilities  
- ECAM validation  

### HID:
- Modifier handling  
- Mouse wheel/buttons  

## 8. AstraOS 0.0.2 Milestone Goals
System should:
- Boot via UEFI  
- Initialize PMM/VMM  
- Initialize interrupt controllers + MSI/MSI-X  
- Enumerate USB devices  
- Handle real HID input  
- Read disk sectors via AHCI  
- Provide debug shell  
- Support multicore  
