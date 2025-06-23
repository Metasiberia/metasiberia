/*=====================================================================
VehiclePhysics.h
----------------
Copyright Glare Technologies Limited 2023 -
=====================================================================*/
#pragma once


#include "PlayerPhysicsInput.h"
#include "PhysicsObject.h"
#include "Scripting.h"
#include "../maths/Vec4f.h"
#include "../maths/vec3.h"


class CameraController;
class PhysicsWorld;


struct VehiclePhysicsUpdateEvents
{
};


/*=====================================================================
VehiclePhysics
--------------
A physics controller for vehicles
=====================================================================*/
class VehiclePhysics : public RefCounted
{
public:
	GLARE_ALIGNED_16_NEW_DELETE

	VehiclePhysics() : last_physics_input_bitflags(0) {}
	virtual ~VehiclePhysics() {}

	virtual WorldObject* getControlledObject() = 0;

	virtual void vehicleSummoned() {} // Set engine revs to zero etc.
	
	virtual void startRightingVehicle() = 0;

	virtual void userEnteredVehicle(int seat_index) = 0; // Should set cur_seat_index

	virtual void userExitedVehicle(int old_seat_index) = 0; // Should set cur_seat_index

	virtual VehiclePhysicsUpdateEvents update(PhysicsWorld& physics_world, const PlayerPhysicsInput& physics_input, float dtime) = 0;

	virtual Vec4f getFirstPersonCamPos(PhysicsWorld& physics_world, uint32 seat_index, bool use_smoothed_network_transform) const = 0;

	virtual Vec4f getThirdPersonCamTargetTranslation() const = 0; // A vector to translate from getFirstPersonCamPos() to where the third person camera should look at.

	virtual float getThirdPersonCamTraceSelfAvoidanceDist() const = 0; // How far to ignore hits for when tracing backwards.

	virtual Matrix4f getBodyTransform(PhysicsWorld& physics_world) const = 0;

	// Return a transformation from seat space to world space.  The transformation should just rotate and translate, but not scale.
	// Sitting position is (0,0,0) in seat space, forwards is (0,1,0), right is (1,0,0)
	virtual Matrix4f getSeatToWorldTransform(PhysicsWorld& physics_world, uint32 seat_index, bool use_smoothed_network_transform) const = 0;

	virtual Vec4f getLinearVel(PhysicsWorld& physics_world) const = 0;

	virtual JPH::BodyID getBodyID() const = 0; // ID of vehicle physics object.

	virtual const Scripting::VehicleScriptedSettings& getSettings() const = 0;

	virtual void setDebugVisEnabled(bool enabled, OpenGLEngine& opengl_engine) {}
	virtual void updateDebugVisObjects() {}

	virtual void updateDopplerEffect(const Vec4f& listener_linear_vel, const Vec4f& listener_pos) {}

	virtual std::string getUIInfoMsg() { return std::string(); }

	uint32 last_physics_input_bitflags;
protected:
};
