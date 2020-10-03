//
//  SMCDellSensors.cpp
//  SMCDellSensors
//
//  Copyright © 2020 lvs1974. All rights reserved.
//

#include <VirtualSMCSDK/kern_vsmcapi.hpp>

#include "SMCDellSensors.hpp"
#include "KeyImplementations.hpp"
#include <Headers/plugin_start.hpp>

#include "kern_hooks.hpp"

OSDefineMetaClassAndStructors(SMCDellSensors, IOService)

bool SMCDellSensors::init(OSDictionary *properties) {
	if (!IOService::init(properties)) {
		return false;
	}

	if (!SMIMonitor::getShared())
		SMIMonitor::createShared();

	SMIMonitor::getShared()->fanMult = 1; //linux proposed to get nominal speed and if it high then change multiplier
	OSNumber * Multiplier = OSDynamicCast(OSNumber, properties->getObject("FanMultiplier"));
	if (Multiplier)
		SMIMonitor::getShared()->fanMult = Multiplier->unsigned32BitValue();
	return true;
}

IOService *SMCDellSensors::probe(IOService *provider, SInt32 *score) {
	auto ptr = IOService::probe(provider, score);
	if (!ptr) {
		SYSLOG("sdell", "failed to probe the parent");
		return nullptr;
	}
	
	if (!SMIMonitor::getShared()->probe())
		return nullptr;
	
	auto fanCount = min(SMIMonitor::getShared()->fanCount, MaxIndexCount);
	VirtualSMCAPI::addKey(KeyFNum, vsmcPlugin.data,
		VirtualSMCAPI::valueWithUint8(fanCount, nullptr, SMC_KEY_ATTRIBUTE_CONST | SMC_KEY_ATTRIBUTE_READ));
	
	for (size_t i = 0; i < fanCount; i++) {
		VirtualSMCAPI::addKey(KeyF0Ac(i), vsmcPlugin.data, VirtualSMCAPI::valueWithFp(0, SmcKeyTypeFpe2, new F0Ac(i), SMC_KEY_ATTRIBUTE_WRITE | SMC_KEY_ATTRIBUTE_READ));
		VirtualSMCAPI::addKey(KeyF0Mn(i), vsmcPlugin.data, VirtualSMCAPI::valueWithFp(0, SmcKeyTypeFpe2, new F0Mn(i), SMC_KEY_ATTRIBUTE_WRITE | SMC_KEY_ATTRIBUTE_READ));
		VirtualSMCAPI::addKey(KeyF0Mx(i), vsmcPlugin.data, VirtualSMCAPI::valueWithFp(0, SmcKeyTypeFpe2, new F0Mx(i), SMC_KEY_ATTRIBUTE_WRITE | SMC_KEY_ATTRIBUTE_READ));
		VirtualSMCAPI::addKey(KeyF0Md(i), vsmcPlugin.data, VirtualSMCAPI::valueWithUint8(0, new F0Md(i), SMC_KEY_ATTRIBUTE_WRITE | SMC_KEY_ATTRIBUTE_READ));
		VirtualSMCAPI::addKey(KeyF0Tg(i), vsmcPlugin.data, VirtualSMCAPI::valueWithFp(0, SmcKeyTypeFpe2, new F0Tg(i), SMC_KEY_ATTRIBUTE_WRITE | SMC_KEY_ATTRIBUTE_READ));
	}
	VirtualSMCAPI::addKey(KeyFS__, vsmcPlugin.data,
		VirtualSMCAPI::valueWithUint16(0, new FS__(), SMC_KEY_ATTRIBUTE_WRITE | SMC_KEY_ATTRIBUTE_READ));

	OSArray* fanNames = OSDynamicCast(OSArray, getProperty("FanNames"));
	char fan_name[DiagFunctionStrLen];

	for (size_t i = 0, cpu = 0; i < fanCount; i++) {
		FanTypeDescStruct	desc;
		FanInfo::SMMFanType type = SMIMonitor::getShared()->state.fanInfo[i].type;
		if (type == FanInfo::Unsupported) {
			auto auto_type = (cpu++ == 0) ? FanInfo::CPU : FanInfo::GPU;
			DBGLOG("sdell", "Fan type %d is unknown, auto assign value %d", type, auto_type);
			type = auto_type;
		}
		snprintf(fan_name, DiagFunctionStrLen, "Fan %lu", i);
		if (fanNames) {
			OSString* name = OSDynamicCast(OSString, fanNames->getObject(type));
			if (name)
				lilu_os_strncpy(fan_name, name->getCStringNoCopy(), DiagFunctionStrLen);
		}
		lilu_os_strncpy(desc.strFunction, fan_name, DiagFunctionStrLen);
		VirtualSMCAPI::addKey(KeyF0ID(i), vsmcPlugin.data, VirtualSMCAPI::valueWithData(
			reinterpret_cast<const SMC_DATA *>(&desc), sizeof(desc), SmcKeyTypeFds, nullptr, SMC_KEY_ATTRIBUTE_CONST | SMC_KEY_ATTRIBUTE_READ));
	}
	
	auto tempCount = min(SMIMonitor::getShared()->tempCount, MaxIndexCount);
	for (size_t i = 0; i < tempCount; i++) {
		TempInfo::SMMTempSensorType type = SMIMonitor::getShared()->state.tempInfo[i].type;
		if (type <= TempInfo::Unsupported || type >= TempInfo::Last) {
			DBGLOG("sdell", "Temp sensor type %d is unknown, auto assign value %d", type, SMIMonitor::getShared()->state.tempInfo[i].index);
			type = static_cast<TempInfo::SMMTempSensorType>(SMIMonitor::getShared()->state.tempInfo[i].index);
		}
		
		switch (type)
		{
		case TempInfo::CPU:
			VirtualSMCAPI::addKey(KeyTC0P(0), vsmcPlugin.data, VirtualSMCAPI::valueWithSp(0, SmcKeyTypeSp78, new TC0P(i),
				SMC_KEY_ATTRIBUTE_WRITE|SMC_KEY_ATTRIBUTE_READ));
			break;
		case TempInfo::GPU:
			VirtualSMCAPI::addKey(KeyTG0P(0), vsmcPlugin.data, VirtualSMCAPI::valueWithSp(0, SmcKeyTypeSp78, new TG0P(i),
				SMC_KEY_ATTRIBUTE_WRITE|SMC_KEY_ATTRIBUTE_READ));
			break;
		case TempInfo::Memory:
			VirtualSMCAPI::addKey(KeyTm0P(0), vsmcPlugin.data, VirtualSMCAPI::valueWithSp(0, SmcKeyTypeSp78, new Tm0P(i),
				SMC_KEY_ATTRIBUTE_WRITE|SMC_KEY_ATTRIBUTE_READ));
			break;
		case TempInfo::Misc:
			VirtualSMCAPI::addKey(KeyTN0P(0), vsmcPlugin.data, VirtualSMCAPI::valueWithSp(0, SmcKeyTypeSp78, new TN0P(i),
				SMC_KEY_ATTRIBUTE_WRITE|SMC_KEY_ATTRIBUTE_READ));
			break;
		case TempInfo::Ambient:
			VirtualSMCAPI::addKey(KeyTA0P(0), vsmcPlugin.data, VirtualSMCAPI::valueWithSp(0, SmcKeyTypeSp78, new TA0P(i),
				SMC_KEY_ATTRIBUTE_WRITE|SMC_KEY_ATTRIBUTE_READ));
			break;
		case TempInfo::Other:
			VirtualSMCAPI::addKey(KeyTW0P(0), vsmcPlugin.data, VirtualSMCAPI::valueWithSp(0, SmcKeyTypeSp78, new TW0P(i),
				SMC_KEY_ATTRIBUTE_WRITE|SMC_KEY_ATTRIBUTE_READ));
			break;
		default:
			DBGLOG("sdell", "Temp sensor type %d is unsupported", type);
			break;
		}
	}
	
	qsort(const_cast<VirtualSMCKeyValue *>(vsmcPlugin.data.data()), vsmcPlugin.data.size(), sizeof(VirtualSMCKeyValue), VirtualSMCKeyValue::compare);
	
	return this;
}

bool SMCDellSensors::start(IOService *provider) {
	if (!IOService::start(provider)) {
		SYSLOG("sdell", "failed to start the parent");
		return false;
	}
	
	SMIMonitor::getShared()->start();
	
	PMinit();
	provider->joinPMtree(this);
	registerPowerDriver(this, powerStates, arrsize(powerStates));
	
	ADDPR(startSuccess) = true;
	
	vsmcNotifier = VirtualSMCAPI::registerHandler(vsmcNotificationHandler, this);
	return vsmcNotifier != nullptr;
}

bool SMCDellSensors::vsmcNotificationHandler(void *sensors, void *refCon, IOService *vsmc, IONotifier *notifier) {
	if (sensors && vsmc) {
		DBGLOG("sdell", "got vsmc notification");
		auto &plugin = static_cast<SMCDellSensors *>(sensors)->vsmcPlugin;
		auto ret = vsmc->callPlatformFunction(VirtualSMCAPI::SubmitPlugin, true, sensors, &plugin, nullptr, nullptr);
		if (ret == kIOReturnSuccess) {
			DBGLOG("sdell", "submitted plugin");
			return true;
		} else if (ret != kIOReturnUnsupported) {
			SYSLOG("sdell", "plugin submission failure %X", ret);
		} else {
			DBGLOG("sdell", "plugin submission to non vsmc");
		}
	} else {
		SYSLOG("sdell", "got null vsmc notification");
	}
	return false;
}

void SMCDellSensors::stop(IOService *provider) {
	PANIC("sdell", "called stop!!!");
}

IOReturn SMCDellSensors::setPowerState(unsigned long state, IOService *whatDevice){
	DBGLOG("sdell", "changing power state to %lu", state);

	if (state == PowerStateOff)
		SMIMonitor::getShared()->handlePowerOff();
	else if (state == PowerStateOn)
		SMIMonitor::getShared()->handlePowerOn();

	return kIOPMAckImplied;
}

