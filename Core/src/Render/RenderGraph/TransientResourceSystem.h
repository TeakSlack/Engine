#ifndef TRANSIENT_RESOURCE_SYSTEM_H
#define TRANSIENT_RESOURCE_SYSTEM_H

class IGpuDevice;
struct RGResourceNode;

class TransientResourceSystem
{
public:
	void Acquire(IGpuDevice* device, RGResourceNode* resource);
	void Release(IGpuDevice* device, RGResourceNode* resource);
};

#endif // TRANSIENT_RESOURCE_SYSTEM_H