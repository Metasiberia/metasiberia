/*=====================================================================
PlayerPhysics.cpp
-----------------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#include "PlayerPhysics.h"


#include "CameraController.h"
#include "PhysicsWorld.h"
#include "PhysicsObject.h"
#include "../shared/WorldObject.h"
#include "../shared/LuaScriptEvaluator.h"
#include "JoltUtils.h"
#include <StringUtils.h>
#include <ConPrint.h>
#include <PlatformUtils.h>
#include <Jolt/Jolt.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>


static const float run_factor = 5; // How much faster you move when the run button (shift) is held down.
static const float move_speed = 3;
static const float jump_speed = 4.5;
static const float max_air_speed = 8;

static const float JUMP_PERIOD = 0.1f; // Allow a jump command to be executed even if the player is not quite on the ground yet.

static const float SPHERE_RAD = 0.3f;
static const float CYLINDER_HEIGHT = 1.3f; // Chosen so the capsule top is about the same height as the head of xbot.glb.  Can test this by jumping into an overhead ledge :)
static const float EYE_HEIGHT = 1.67f;


PlayerPhysics::PlayerPhysics()
:	move_desired_vel(0,0,0),
	last_jump_time(-1),
	on_ground(false),
	fly_mode(false),
	last_runpressed(false),
	//time_since_on_ground(0),
	campos_z_delta(0),
	gravity_enabled(true)
{
}


PlayerPhysics::~PlayerPhysics()
{
}


static const float cCharacterHeightStanding = CYLINDER_HEIGHT;
static const float cCharacterHeightSitting = 0.3f;
static const float cCharacterRadiusStanding = SPHERE_RAD;


void PlayerPhysics::init(PhysicsWorld& physics_world, const Vec3d& initial_player_pos)
{
	physics_system = physics_world.physics_system;

	// Jolt position is at the bottom of the character controller, substrata position is at eye level.
	const Vec3d use_player_pos = initial_player_pos - Vec3d(0, 0, EYE_HEIGHT);

	// Create virtual character
	{
		standing_shape = JPH::RotatedTranslatedShapeSettings(
			JPH::Vec3(0, 0, 0.5f * cCharacterHeightStanding + cCharacterRadiusStanding), // position
			JPH::Quat::sRotation(JPH::Vec3(1, 0, 0), Maths::pi_2<float>()), // rotate capsule from extending in the y-axis to the z-axis.
			new JPH::CapsuleShape(/*inHalfHeightOfCylinder=*/0.5f * cCharacterHeightStanding, /*inRadius=*/cCharacterRadiusStanding)).Create().Get();

		sitting_shape = JPH::RotatedTranslatedShapeSettings(
			JPH::Vec3(0, 0, 0.5f * cCharacterHeightSitting + cCharacterRadiusStanding), // position
			JPH::Quat::sRotation(JPH::Vec3(1, 0, 0), Maths::pi_2<float>()), // rotate capsule from extending in the y-axis to the z-axis.
			new JPH::CapsuleShape(/*inHalfHeightOfCylinder=*/0.5f * cCharacterHeightSitting, /*inRadius=*/cCharacterRadiusStanding)).Create().Get();

		JPH::Ref<JPH::CharacterVirtualSettings> settings = new JPH::CharacterVirtualSettings();
		settings->mShape = standing_shape;
		settings->mUp = JPH::Vec3(0, 0, 1); // Set world-space up vector
		settings->mSupportingVolume = JPH::Plane(JPH::Vec3(0,0,1), -SPHERE_RAD); // Accept contacts that touch the lower sphere of the capsule
		settings->mMaxStrength = 1000; // Default pushing force is 100 N, which doesn't seem enough.

		jolt_character = new JPH::CharacterVirtual(settings, toJoltVec3(use_player_pos), JPH::Quat::sIdentity(), physics_world.physics_system);

		jolt_character->SetListener(this);
	}

	// NOTE Jolt now supports detecting contacts between CharacterVirtual and sensor objects.  So we don't need an interaction character for that.
}


void PlayerPhysics::shutdown()
{
	jolt_character = NULL;
	standing_shape = NULL;
	sitting_shape = NULL;
}


void PlayerPhysics::setStandingShape(PhysicsWorld& physics_world)
{
	jolt_character->SetShape(standing_shape, FLT_MAX, physics_world.physics_system->GetDefaultBroadPhaseLayerFilter(Layers::MOVING), 
		physics_world.physics_system->GetDefaultLayerFilter(Layers::MOVING), /*player_physics_body_filter*/{}, {}, *physics_world.temp_allocator);
}


void PlayerPhysics::setSittingShape(PhysicsWorld& physics_world)
{
	jolt_character->SetShape(sitting_shape, FLT_MAX, physics_world.physics_system->GetDefaultBroadPhaseLayerFilter(Layers::MOVING), 
		physics_world.physics_system->GetDefaultLayerFilter(Layers::MOVING), /*player_physics_body_filter*/{}, {}, *physics_world.temp_allocator);
}


Vec3d PlayerPhysics::getCapsuleBottomPosition()
{
	// Jolt position is at the bottom of the character controller, substrata position is at eye level.
	if(jolt_character)
		return toVec3d(toVec3f(jolt_character->GetPosition()));
	else
		return Vec3d(0.0);
}


void PlayerPhysics::setEyePosition(const Vec3d& new_player_pos, const Vec4f& linear_vel) // Move discontinuously.  For teleporting etc.
{
	// Jolt position is at the bottom of the character controller, substrata position is at eye level.
	setCapsuleBottomPosition(new_player_pos - Vec3d(0, 0, EYE_HEIGHT));
}

void PlayerPhysics::setCapsuleBottomPosition(const Vec3d& new_player_pos, const Vec4f& linear_vel)
{
	// Jolt position is at the bottom of the character controller, substrata position is at eye level.
	if(jolt_character)
	{
		jolt_character->SetPosition(toJoltVec3(new_player_pos));
		jolt_character->SetLinearVelocity(toJoltVec3(linear_vel));
	}
}


float PlayerPhysics::getEyeHeight()
{
	return EYE_HEIGHT;
}


inline float doRunFactor(bool runpressed)
{
	if(runpressed)
		return run_factor;
	else
		return 1.0f;
}


void PlayerPhysics::processMoveForwards(float factor, bool runpressed, CameraController& cam)
{
	last_runpressed = runpressed;
	move_desired_vel += ::toVec3f(cam.getForwardsVec()) * factor * move_speed * doRunFactor(runpressed);

	// When the player spawns, gravity will be turned off, so they don't e.g. fall through buildings before they have been loaded.
	// Turn it on as soon as the player tries to move.
	this->gravity_enabled = true;
}


void PlayerPhysics::processStrafeRight(float factor, bool runpressed, CameraController& cam)
{
	last_runpressed = runpressed;
	move_desired_vel += ::toVec3f(cam.getRightVec()) * factor * move_speed * doRunFactor(runpressed);

	this->gravity_enabled = true;
}


void PlayerPhysics::processMoveUp(float factor, bool runpressed, CameraController& cam)
{
	last_runpressed = runpressed;
	if(fly_mode)
		move_desired_vel += Vec3f(0,0,1) * factor * move_speed * doRunFactor(runpressed);
}


void PlayerPhysics::processJump(CameraController& cam, double cur_time)
{
	last_jump_time = cur_time;

	this->gravity_enabled = true;
}


void PlayerPhysics::setFlyModeEnabled(bool enabled)
{
	fly_mode = enabled;
}


/*static const std::string getGroundStateName(JPH::CharacterBase::EGroundState s)
{
	switch(s)
	{
	case JPH::CharacterBase::EGroundState::OnGround: return "OnGround";
	case JPH::CharacterBase::EGroundState::OnSteepGround: return "OnSteepGround";
	case JPH::CharacterBase::EGroundState::NotSupported: return "NotSupported";
	case JPH::CharacterBase::EGroundState::InAir: return "InAir";
	default: return "unknown";
	}
}*/


// We don't want the virtual character to collide with the pusher object.
class PlayerPhysicsObjectLayerFilter : public JPH::ObjectLayerFilter
{
public:
	// Function to filter out object layers when doing collision query test (return true to allow testing against objects with this layer)
	virtual bool ShouldCollide(JPH::ObjectLayer inLayer) const
	{
		return /*inLayer != Layers::PUSHER_CHARACTER && */inLayer != Layers::NON_COLLIDABLE;
	}
};


UpdateEvents PlayerPhysics::update(PhysicsWorld& physics_world, const PlayerPhysicsInput& physics_input, float dtime, double cur_time, Vec4f& campos_out)
{
	UpdateEvents events;

	Vec3f vel = toVec3f(jolt_character->GetLinearVelocity());

	// Apply movement forces
	if(!fly_mode) // if not flying
	{
		Vec3f parralel_vel = move_desired_vel;
		parralel_vel.z = 0;

		jolt_character->UpdateGroundVelocity(); // Get updated ground velocity.  Helps reduce jitter on platforms etc.

		if(jolt_character->IsSupported() && // GetGroundState() == JPH::CharacterVirtual::EGroundState::OnGround)
			((vel.z - jolt_character->GetGroundVelocity().GetZ()) < 0.1f)) // And not moving away from ground.  (Need this because sometimes IsSupported() is true after we jumped)
		{
			vel = parralel_vel; // When on the ground, set velocity instantly to the desired velocity.
			vel += toVec3f(jolt_character->GetGroundVelocity()); // Add ground velocity, so player will move with a platform they are standing on.
		}
		else
		{
			if(parralel_vel.length() > max_air_speed) // maxairspeed is really max acceleration in air.
				parralel_vel.setLength(max_air_speed);
			vel += parralel_vel * dtime; // Acclerate in desired direction.
		}

		// Apply gravity, even when we are on the ground (supported).  Applying gravity when on ground seems to be important for preventing being InAir occasionally while riding platforms.
		if(gravity_enabled)
		{
			const Vec3f gravity_accel(0, 0, -9.81f); 
			vel += gravity_accel * dtime;
		}

		if(vel.z < -100) // cap falling speed at 100 m/s
			vel.z = -100;
	}
	else // Else if flying:
	{
		// Desired velocity is maintaining the current speed but pointing in the moveimpulse direction.
		const float speed = vel.length();
		const Vec3f desired_vel = (move_desired_vel.length() < 1.e-4f) ? Vec3f(0.f) : (normalise(move_desired_vel) * speed);

		const Vec3f accel = move_desired_vel * 3.f
			+ (desired_vel - vel) * 2.f;

		vel += accel * dtime;
	}

	campos_z_delta = campos_z_delta - 20.f * dtime * campos_z_delta; // Exponentially reduce campos_z_delta over time until it reaches 0.
	if(std::fabs(campos_z_delta) < 1.0e-5f)
		campos_z_delta = 0;

	this->on_ground = jolt_character->IsSupported() && 
		((jolt_character->GetLinearVelocity().GetZ() - jolt_character->GetGroundVelocity().GetZ()) < 0.1f); // And not moving away from ground.  (Need this because sometimes IsSupported() is true after we jumped)

	// Jump
	const double time_since_jump_pressed = cur_time - last_jump_time;
	if((time_since_jump_pressed < JUMP_PERIOD) &&
		jolt_character->IsSupported()) // jolt_character->GetGroundState() == JPH::CharacterVirtual::EGroundState::OnGround) // If on ground
	{
		//conPrint("JUMPING");
		on_ground = false;

		// Recompute vel using proper ground normal.  Needed otherwise jumping while running uphill doesn't work properly.
		const Vec3f ground_normal = toVec3f(jolt_character->GetGroundNormal());
		if(fly_mode)
			vel += Vec3f(0, 0, jump_speed); // If flying, maintain sideways velocity.
		else
			vel = removeComponentInDir(move_desired_vel, ground_normal) + 
				toVec3f(jolt_character->GetGroundVelocity()) + 
				Vec3f(0, 0, jump_speed);

		last_jump_time = -1;
		events.jumped = true;
		//time_since_on_ground = 1; // Hack this up to a large value so jump animation can play immediately.
	}

	jolt_character->SetLinearVelocity(JPH::Vec3(vel.x, vel.y, vel.z));

	JPH::CharacterVirtual::ExtendedUpdateSettings settings;
	settings.mStickToFloorStepDown		= JPH::Vec3(0, 0, -0.5f);
	settings.mWalkStairsStepUp			= JPH::Vec3(0.0f, 0.0f, 0.4f);

	PlayerPhysicsObjectLayerFilter player_physics_filter;

	// Put the guts of ExtendedUpdate here, just so we can extract pre_stair_walk_position from the middle of it.
#if 1
	jolt_character->ExtendedUpdate(dtime, physics_world.physics_system->GetGravity(), settings, physics_world.physics_system->GetDefaultBroadPhaseLayerFilter(Layers::MOVING), 
		physics_world.physics_system->GetDefaultLayerFilter(Layers::MOVING), /*player_physics_body_filter*/{}, {}, *physics_world.temp_allocator);

	const JPH::Vec3 pre_stair_walk_position = jolt_character->GetPosition();
#else
	//----------------------------------------- ExtendedUpdate --------------------------------------
	const float inDeltaTime = dtime;
	const JPH::CharacterVirtual::ExtendedUpdateSettings inSettings = settings;
	const JPH::Vec3 inGravity = physics_world.physics_system->GetGravity();
	const JPH::BroadPhaseLayerFilter &inBroadPhaseLayerFilter = physics_world.physics_system->GetDefaultBroadPhaseLayerFilter(Layers::MOVING);
	const JPH::ObjectLayerFilter &inObjectLayerFilter = player_physics_filter;

	const JPH::BodyFilter &inBodyFilter = player_physics_body_filter;
	const JPH::ShapeFilter &inShapeFilter = {};
	JPH::TempAllocator &inAllocator = *physics_world.temp_allocator;

	const JPH::Vec3 mUp = jolt_character->GetUp();

	// Update the velocity
	JPH::Vec3 desired_velocity = jolt_character->GetLinearVelocity();
	jolt_character->SetLinearVelocity(jolt_character->CancelVelocityTowardsSteepSlopes(desired_velocity));

	// Remember old position
	JPH::RVec3 old_position = jolt_character->GetPosition();

	// Track if on ground before the update
	bool ground_to_air = jolt_character->IsSupported();

	// Update the character position (instant, do not have to wait for physics update)
	jolt_character->Update(inDeltaTime, inGravity, inBroadPhaseLayerFilter, inObjectLayerFilter, inBodyFilter, inShapeFilter, inAllocator);

	// ... and that we got into air after
	if (jolt_character->IsSupported())
		ground_to_air = false;

	const JPH::Vec3 pre_stair_walk_position = jolt_character->GetPosition(); // NICK NEW

	// If stick to floor enabled and we're going from supported to not supported
	if (ground_to_air && !inSettings.mStickToFloorStepDown.IsNearZero())
	{
		// If we're not moving up, stick to the floor
		float velocity = JPH::Vec3(jolt_character->GetPosition() - old_position).Dot(mUp) / inDeltaTime;
		if (velocity <= 1.0e-6f)
			jolt_character->StickToFloor(inSettings.mStickToFloorStepDown, inBroadPhaseLayerFilter, inObjectLayerFilter, inBodyFilter, inShapeFilter, inAllocator);
	}

	// If walk stairs enabled
	if (!inSettings.mWalkStairsStepUp.IsNearZero())
	{
		// Calculate how much we wanted to move horizontally
		JPH::Vec3 desired_horizontal_step = desired_velocity * inDeltaTime;
		desired_horizontal_step -= desired_horizontal_step.Dot(mUp) * mUp;
		float desired_horizontal_step_len = desired_horizontal_step.Length();
		if (desired_horizontal_step_len > 0.0f)
		{
			// Calculate how much we moved horizontally
			JPH::Vec3 achieved_horizontal_step = JPH::Vec3(jolt_character->GetPosition() - old_position);
			achieved_horizontal_step -= achieved_horizontal_step.Dot(mUp) * mUp;

			// Only count movement in the direction of the desired movement
			// (otherwise we find it ok if we're sliding downhill while we're trying to climb uphill)
			JPH::Vec3 step_forward_normalized = desired_horizontal_step / desired_horizontal_step_len;
			achieved_horizontal_step = std::max(0.0f, achieved_horizontal_step.Dot(step_forward_normalized)) * step_forward_normalized;
			float achieved_horizontal_step_len = achieved_horizontal_step.Length();

			// If we didn't move as far as we wanted and we're against a slope that's too steep
			if (achieved_horizontal_step_len + 1.0e-4f < desired_horizontal_step_len
				&& jolt_character->CanWalkStairs(desired_velocity))
			{
				// Calculate how much we should step forward
				// Note that we clamp the step forward to a minimum distance. This is done because at very high frame rates the delta time
				// may be very small, causing a very small step forward. If the step becomes small enough, we may not move far enough
				// horizontally to actually end up at the top of the step.
				JPH::Vec3 step_forward = step_forward_normalized * std::max(inSettings.mWalkStairsMinStepForward, desired_horizontal_step_len - achieved_horizontal_step_len);

				// Calculate how far to scan ahead for a floor. This is only used in case the floor normal at step_forward is too steep.
				// In that case an additional check will be performed at this distance to check if that normal is not too steep.
				// Start with the ground normal in the horizontal plane and normalizing it
				JPH::Vec3 step_forward_test = -jolt_character->GetGroundNormal();
				step_forward_test -= step_forward_test.Dot(mUp) * mUp;
				step_forward_test = step_forward_test.NormalizedOr(step_forward_normalized);

				// If this normalized vector and the character forward vector is bigger than a preset angle, we use the character forward vector instead of the ground normal
				// to do our forward test
				if (step_forward_test.Dot(step_forward_normalized) < inSettings.mWalkStairsCosAngleForwardContact)
					step_forward_test = step_forward_normalized;

				// Calculate the correct magnitude for the test vector
				step_forward_test *= inSettings.mWalkStairsStepForwardTest;

				jolt_character->WalkStairs(inDeltaTime, inSettings.mWalkStairsStepUp, step_forward, step_forward_test, inSettings.mWalkStairsStepDownExtra, inBroadPhaseLayerFilter, inObjectLayerFilter, inBodyFilter, inShapeFilter, inAllocator);
			}
		}
	}
	// ----------------------------------------- End ExtendedUpdate --------------------------------------
#endif


	const float dz = jolt_character->GetPosition().GetZ() - pre_stair_walk_position.GetZ();
	campos_z_delta = myClamp(campos_z_delta + dz, -0.3f, 0.3f);

	if(jolt_character->IsSupported())
		this->last_xy_plane_vel_rel_ground = toVec3f(jolt_character->GetLinearVelocity() - jolt_character->GetGroundVelocity());
	else
		this->last_xy_plane_vel_rel_ground = toVec3f(jolt_character->GetLinearVelocity());
	this->last_xy_plane_vel_rel_ground.z = 0;


	const JPH::Vec3 char_pos = jolt_character->GetPosition();
	campos_out = Vec4f(char_pos.GetX(), char_pos.GetY(), char_pos.GetZ() + EYE_HEIGHT - campos_z_delta, 1.f);

	//if(!/*onground*/(jolt_character->GetGroundState() == JPH::CharacterVirtual::EGroundState::OnGround))
	//	time_since_on_ground += dtime;
	//else
	//	time_since_on_ground = 0;

	return events;
}


// Just run a basic CharacterVirtual Update(), so that collisions with sensors are detected.
// This means we can still trigger contacts with sensor objects while in a vehicle.
// Collisions with vehicle_body_id will be ignored
void PlayerPhysics::updateForInVehicle(PhysicsWorld& physics_world, const PlayerPhysicsInput& physics_input, float dtime, JPH::BodyID vehicle_body_id)
{
	JPH::IgnoreSingleBodyFilter player_physics_body_filter(vehicle_body_id); // Don't collide with the vehicle we are inside of

	jolt_character->Update(dtime, physics_world.physics_system->GetGravity(), physics_world.physics_system->GetDefaultBroadPhaseLayerFilter(Layers::MOVING), 
		physics_world.physics_system->GetDefaultLayerFilter(Layers::MOVING), /*body filter=*/player_physics_body_filter, /*shape filter=*/{}, *physics_world.temp_allocator);
}


Vec4f PlayerPhysics::getLinearVel() const
{
	return toVec4fVec(jolt_character->GetLinearVelocity());
}


void PlayerPhysics::setLinearVel(const Vec4f& new_linear_vel)
{
	if(jolt_character)
		jolt_character->SetLinearVelocity(toJoltVec3(new_linear_vel));
}


void PlayerPhysics::addToLinearVel(const Vec4f& delta_v)
{
	if(jolt_character)
	{
		const Vec4f new_linear_vel = getLinearVel() + delta_v;
		jolt_character->SetLinearVelocity(toJoltVec3(new_linear_vel));
	}
}


bool PlayerPhysics::isMoveDesiredVelNonZero()
{
	return move_desired_vel.length2() != 0;
}


void PlayerPhysics::zeroMoveDesiredVel()
{
	move_desired_vel.set(0,0,0);
}


void PlayerPhysics::OnContactAdded(const JPH::CharacterVirtual *inCharacter, const JPH::BodyID &inBodyID2, const JPH::SubShapeID &inSubShapeID2, JPH::RVec3Arg inContactPosition, JPH::Vec3Arg inContactNormal, JPH::CharacterContactSettings &ioSettings)
{
	JPH::BodyLockRead lock(physics_system->GetBodyLockInterface(), inBodyID2);
	if(lock.Succeeded())
	{
		const JPH::Body& body = lock.GetBody();
		const uint64 user_data = body.GetUserData();
		if(user_data != 0)
		{
			PhysicsObject* physics_ob = (PhysicsObject*)user_data;
			contacted_events.push_back(ContactedEvent({physics_ob}));
		}
	}
}


void PlayerPhysics::debugGetCollisionSpheres(const Vec4f& campos, std::vector<js::BoundingSphere>& spheres_out)
{
	// Visualise virtual character object.  The character object is a capsule, but visualise as 3 spheres.
	spheres_out.resize(0);
	
	const float cylinder_height = (jolt_character->GetShape() == this->standing_shape) ? cCharacterHeightStanding : cCharacterHeightSitting;

	if(jolt_character)
	{
		spheres_out.push_back(js::BoundingSphere((toVec3f(jolt_character->GetPosition()) + Vec3f(0, 0, SPHERE_RAD)).toVec4fPoint(),								SPHERE_RAD));
		spheres_out.push_back(js::BoundingSphere((toVec3f(jolt_character->GetPosition()) + Vec3f(0, 0, SPHERE_RAD + cylinder_height / 2)).toVec4fPoint(),		SPHERE_RAD));
		spheres_out.push_back(js::BoundingSphere((toVec3f(jolt_character->GetPosition()) + Vec3f(0, 0, SPHERE_RAD + cylinder_height)).toVec4fPoint(),			SPHERE_RAD));
	}
}
