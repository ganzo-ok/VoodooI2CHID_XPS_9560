//
//  VoodooI2CHIDDeviceOverride.hpp
//  VoodooI2CHID
//
//  Created by Alexandre Daoud on 10/08/2019.
//  Copyright © 2019 Alexandre Daoud. All rights reserved.
//

#ifndef VoodooI2CHIDDeviceOverride_hpp
#define VoodooI2CHIDDeviceOverride_hpp

#include "VoodooI2CHIDDevice.hpp"

class EXPORT VoodooI2CHIDDeviceOverride : public VoodooI2CHIDDevice {
  OSDeclareDefaultStructors(VoodooI2CHIDDeviceOverride);

 public:
    // Override report descriptors for specific devices (e.g. SYNA3602)
    VoodooI2CHIDDeviceHIDDescriptor hid_descriptor_override;
    UInt8* report_descriptor_override;

    IOReturn getHIDDescriptor() override;
    IOReturn newReportDescriptor(IOMemoryDescriptor** descriptor) const override;
    
    // REMOVED: willTerminate and didTerminate overrides.
    // Inheriting VoodooI2CHIDDevice::didTerminate (which calls IOService::didTerminate) is correct.
};

#endif /* VoodooI2CHIDDeviceOverride_hpp */