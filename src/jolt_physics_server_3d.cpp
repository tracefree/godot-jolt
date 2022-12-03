#include "jolt_physics_server_3d.hpp"

#include "error_macros.hpp"
#include "jolt_physics_area_3d.hpp"
#include "jolt_physics_body_3d.hpp"
#include "jolt_physics_group_filter.hpp"
#include "jolt_physics_shape_3d.hpp"
#include "jolt_physics_space_3d.hpp"
#include "utility_functions.hpp"

namespace {

constexpr int32_t GDJOLT_MAX_PHYSICS_JOBS = 2048;
constexpr int32_t GDJOLT_MAX_PHYSICS_BARRIERS = 8;

void* jolt_alloc(size_t p_size) {
	return mi_malloc(p_size);
}

void jolt_free(void* p_mem) {
	mi_free(p_mem);
}

void* jolt_aligned_alloc(size_t p_size, size_t p_alignment) {
	return mi_aligned_alloc(p_alignment, p_size);
}

void jolt_aligned_free(void* p_mem) {
	mi_free(p_mem);
}

void jolt_trace(const char* p_format, ...) {
	va_list args; // NOLINT(cppcoreguidelines-init-variables)
	va_start(args, p_format);
	char buffer[1024] = {'\0'};
	vsnprintf(buffer, sizeof(buffer), p_format, args);
	va_end(args);
	UtilityFunctions::print_verbose(buffer);
}

#ifdef JPH_ENABLE_ASSERTS

bool jolt_assert(const char* p_expr, const char* p_msg, const char* p_file, uint32_t p_line) {
	ERR_PRINT(vformat(
		"Jolt assertion failed: {}:{} ({}) {}",
		p_file,
		p_line,
		p_expr,
		p_msg != nullptr ? p_msg : ""
	));

	return false;
}

#endif // JPH_ENABLE_ASSERTS

} // anonymous namespace

void JoltPhysicsServer3D::init_statics() {
	JPH::Allocate = &jolt_alloc;
	JPH::Free = &jolt_free;
	JPH::AlignedAllocate = &jolt_aligned_alloc;
	JPH::AlignedFree = &jolt_aligned_free;
	JPH::Trace = &jolt_trace;

	JPH_IF_ENABLE_ASSERTS(JPH::AssertFailed = jolt_assert;)

	JPH::Factory::sInstance = new JPH::Factory();

	JPH::RegisterTypes();

	job_system = new JPH::JobSystemThreadPool(
		GDJOLT_MAX_PHYSICS_JOBS,
		GDJOLT_MAX_PHYSICS_BARRIERS,
		(int)std::thread::hardware_concurrency() - 1
	);

	group_filter = new JoltPhysicsGroupFilter();
}

void JoltPhysicsServer3D::finish_statics() {
	group_filter = nullptr;

	delete job_system;
	job_system = nullptr;

	// HACK(mihe): We don't want this to destruct at DLL exit since we won't have any allocators
	// assigned at that point, so we free it explicitly here instead
	JPH::PhysicsMaterial::sDefault = nullptr;

	delete JPH::Factory::sInstance;
	JPH::Factory::sInstance = nullptr;

	JPH_IF_ENABLE_ASSERTS(JPH::AssertFailed = nullptr;)

	JPH::Allocate = nullptr;
	JPH::Free = nullptr;
	JPH::AlignedAllocate = nullptr;
	JPH::AlignedFree = nullptr;
	JPH::Trace = nullptr;
}

RID JoltPhysicsServer3D::_world_boundary_shape_create() {
	ERR_FAIL_D_NOT_IMPL();
}

RID JoltPhysicsServer3D::_separation_ray_shape_create() {
	ERR_FAIL_D_NOT_IMPL();
}

RID JoltPhysicsServer3D::_sphere_shape_create() {
	JoltPhysicsShape3D* shape = memnew(JoltPhysicsSphereShape3D);
	RID rid = shape_owner.make_rid(shape);
	shape->set_rid(rid);
	return rid;
}

RID JoltPhysicsServer3D::_box_shape_create() {
	JoltPhysicsShape3D* shape = memnew(JoltPhysicsBoxShape3D);
	RID rid = shape_owner.make_rid(shape);
	shape->set_rid(rid);
	return rid;
}

RID JoltPhysicsServer3D::_capsule_shape_create() {
	ERR_FAIL_D_NOT_IMPL();
}

RID JoltPhysicsServer3D::_cylinder_shape_create() {
	ERR_FAIL_D_NOT_IMPL();
}

RID JoltPhysicsServer3D::_convex_polygon_shape_create() {
	ERR_FAIL_D_NOT_IMPL();
}

RID JoltPhysicsServer3D::_concave_polygon_shape_create() {
	ERR_FAIL_D_NOT_IMPL();
}

RID JoltPhysicsServer3D::_heightmap_shape_create() {
	ERR_FAIL_D_NOT_IMPL();
}

RID JoltPhysicsServer3D::_custom_shape_create() {
	ERR_FAIL_D_NOT_IMPL();
}

void JoltPhysicsServer3D::_shape_set_data(const RID& p_shape, const Variant& p_data) {
	JoltPhysicsShape3D* shape = shape_owner.get_or_null(p_shape);
	ERR_FAIL_NULL(shape);

	shape->set_data(p_data);
}

void JoltPhysicsServer3D::_shape_set_custom_solver_bias(
	[[maybe_unused]] const RID& p_shape,
	[[maybe_unused]] double p_bias
) {
	ERR_FAIL_NOT_IMPL();
}

PhysicsServer3D::ShapeType JoltPhysicsServer3D::_shape_get_type([[maybe_unused]] const RID& p_shape
) const {
	ERR_FAIL_D_NOT_IMPL();
}

Variant JoltPhysicsServer3D::_shape_get_data(const RID& p_shape) const {
	JoltPhysicsShape3D* shape = shape_owner.get_or_null(p_shape);
	ERR_FAIL_NULL_D(shape);

	return shape->get_data();
}

void JoltPhysicsServer3D::_shape_set_margin(
	[[maybe_unused]] const RID& p_shape,
	[[maybe_unused]] double p_margin
) {
	ERR_FAIL_NOT_IMPL();
}

double JoltPhysicsServer3D::_shape_get_margin([[maybe_unused]] const RID& p_shape) const {
	ERR_FAIL_D_NOT_IMPL();
}

double JoltPhysicsServer3D::_shape_get_custom_solver_bias([[maybe_unused]] const RID& p_shape
) const {
	ERR_FAIL_D_NOT_IMPL();
}

RID JoltPhysicsServer3D::_space_create() {
	JoltPhysicsSpace3D* space = memnew(JoltPhysicsSpace3D(job_system, group_filter));
	RID rid = space_owner.make_rid(space);
	space->set_rid(rid);

	const RID default_area_rid = area_create();
	JoltPhysicsArea3D* default_area = area_owner.get_or_null(default_area_rid);
	ERR_FAIL_NULL_D(default_area);
	space->set_default_area(default_area);
	default_area->set_space(space);

	return rid;
}

void JoltPhysicsServer3D::_space_set_active(const RID& p_space, bool p_active) {
	JoltPhysicsSpace3D* space = space_owner.get_or_null(p_space);
	ERR_FAIL_NULL(space);

	if (p_active) {
		active_spaces.insert(space);
	} else {
		active_spaces.erase(space);
	}
}

bool JoltPhysicsServer3D::_space_is_active(const RID& p_space) const {
	JoltPhysicsSpace3D* space = space_owner.get_or_null(p_space);
	ERR_FAIL_NULL_D(space);

	return active_spaces.has(space);
}

void JoltPhysicsServer3D::_space_set_param(
	[[maybe_unused]] const RID& p_space,
	[[maybe_unused]] SpaceParameter p_param,
	[[maybe_unused]] double p_value
) {
	ERR_FAIL_NOT_IMPL();
}

double JoltPhysicsServer3D::_space_get_param(
	[[maybe_unused]] const RID& p_space,
	[[maybe_unused]] SpaceParameter p_param
) const {
	ERR_FAIL_D_NOT_IMPL();
}

PhysicsDirectSpaceState3D* JoltPhysicsServer3D::_space_get_direct_state(const RID& p_space) {
	JoltPhysicsSpace3D* space = space_owner.get_or_null(p_space);
	ERR_FAIL_NULL_D(space);
	ERR_FAIL_COND_D_MSG(
		!doing_sync || space->is_locked(),
		"Space state is inaccessible right now, wait for iteration or physics process notification."
	);

	return space->get_direct_state();
}

void JoltPhysicsServer3D::_space_set_debug_contacts(
	[[maybe_unused]] const RID& p_space,
	[[maybe_unused]] int64_t p_max_contacts
) {
	ERR_FAIL_NOT_IMPL();
}

PackedVector3Array JoltPhysicsServer3D::_space_get_contacts([[maybe_unused]] const RID& p_space
) const {
	ERR_FAIL_D_NOT_IMPL();
}

int64_t JoltPhysicsServer3D::_space_get_contact_count([[maybe_unused]] const RID& p_space) const {
	ERR_FAIL_D_NOT_IMPL();
}

RID JoltPhysicsServer3D::_area_create() {
	JoltPhysicsArea3D* area = memnew(JoltPhysicsArea3D);
	RID rid = area_owner.make_rid(area);
	area->set_rid(rid);
	return rid;
}

void JoltPhysicsServer3D::_area_set_space(
	[[maybe_unused]] const RID& p_area,
	[[maybe_unused]] const RID& p_space
) {
	ERR_FAIL_NOT_IMPL();
}

RID JoltPhysicsServer3D::_area_get_space([[maybe_unused]] const RID& p_area) const {
	ERR_FAIL_D_NOT_IMPL();
}

void JoltPhysicsServer3D::_area_add_shape(
	[[maybe_unused]] const RID& p_area,
	[[maybe_unused]] const RID& p_shape,
	[[maybe_unused]] const Transform3D& p_transform,
	[[maybe_unused]] bool p_disabled
) {
	ERR_FAIL_NOT_IMPL();
}

void JoltPhysicsServer3D::_area_set_shape(
	[[maybe_unused]] const RID& p_area,
	[[maybe_unused]] int64_t p_shape_idx,
	[[maybe_unused]] const RID& p_shape
) {
	ERR_FAIL_NOT_IMPL();
}

void JoltPhysicsServer3D::_area_set_shape_transform(
	[[maybe_unused]] const RID& p_area,
	[[maybe_unused]] int64_t p_shape_idx,
	[[maybe_unused]] const Transform3D& p_transform
) {
	ERR_FAIL_NOT_IMPL();
}

int64_t JoltPhysicsServer3D::_area_get_shape_count([[maybe_unused]] const RID& p_area) const {
	ERR_FAIL_D_NOT_IMPL();
}

RID JoltPhysicsServer3D::_area_get_shape(
	[[maybe_unused]] const RID& p_area,
	[[maybe_unused]] int64_t p_shape_idx
) const {
	ERR_FAIL_D_NOT_IMPL();
}

Transform3D JoltPhysicsServer3D::_area_get_shape_transform(
	[[maybe_unused]] const RID& p_area,
	[[maybe_unused]] int64_t p_shape_idx
) const {
	ERR_FAIL_D_NOT_IMPL();
}

void JoltPhysicsServer3D::_area_remove_shape(
	[[maybe_unused]] const RID& p_area,
	[[maybe_unused]] int64_t p_shape_idx
) {
	ERR_FAIL_NOT_IMPL();
}

void JoltPhysicsServer3D::_area_clear_shapes([[maybe_unused]] const RID& p_area) {
	ERR_FAIL_NOT_IMPL();
}

void JoltPhysicsServer3D::_area_set_shape_disabled(
	[[maybe_unused]] const RID& p_area,
	[[maybe_unused]] int64_t p_shape_idx,
	[[maybe_unused]] bool p_disabled
) {
	ERR_FAIL_NOT_IMPL();
}

void JoltPhysicsServer3D::_area_attach_object_instance_id(const RID& p_area, int64_t p_id) {
	JoltPhysicsArea3D* area = area_owner.get_or_null(p_area);
	ERR_FAIL_NULL(area);

	area->set_instance_id(p_id);
}

int64_t JoltPhysicsServer3D::_area_get_object_instance_id(const RID& p_area) const {
	JoltPhysicsArea3D* area = area_owner.get_or_null(p_area);
	ERR_FAIL_NULL_V(area, -1);

	return area->get_instance_id();
}

void JoltPhysicsServer3D::_area_set_param(
	const RID& p_area,
	AreaParameter p_param,
	const Variant& p_value
) {
	RID area_rid = p_area;

	if (space_owner.owns(area_rid)) {
		JoltPhysicsSpace3D* space = space_owner.get_or_null(area_rid);
		area_rid = space->get_default_area()->get_rid();
	}

	JoltPhysicsArea3D* area = area_owner.get_or_null(area_rid);
	ERR_FAIL_NULL(area);

	area->set_param(p_param, p_value);
}

void JoltPhysicsServer3D::_area_set_transform(
	[[maybe_unused]] const RID& p_area,
	[[maybe_unused]] const Transform3D& p_transform
) {
	ERR_FAIL_NOT_IMPL();
}

Variant JoltPhysicsServer3D::_area_get_param(
	[[maybe_unused]] const RID& p_area,
	[[maybe_unused]] AreaParameter p_param
) const {
	ERR_FAIL_D_NOT_IMPL();
}

Transform3D JoltPhysicsServer3D::_area_get_transform([[maybe_unused]] const RID& p_area) const {
	ERR_FAIL_D_NOT_IMPL();
}

void JoltPhysicsServer3D::_area_set_collision_mask(
	[[maybe_unused]] const RID& p_area,
	[[maybe_unused]] int64_t p_mask
) {
	ERR_FAIL_NOT_IMPL();
}

void JoltPhysicsServer3D::_area_set_collision_layer(
	[[maybe_unused]] const RID& p_area,
	[[maybe_unused]] int64_t p_layer
) {
	ERR_FAIL_NOT_IMPL();
}

void JoltPhysicsServer3D::_area_set_monitorable(const RID& p_area, bool p_monitorable) {
	JoltPhysicsArea3D* area = area_owner.get_or_null(p_area);
	ERR_FAIL_NULL(area);

	area->set_monitorable(p_monitorable);
}

void JoltPhysicsServer3D::_area_set_monitor_callback(
	const RID& p_area,
	const Callable& p_callback
) {
	JoltPhysicsArea3D* area = area_owner.get_or_null(p_area);
	ERR_FAIL_NULL(area);

	area->set_monitor_callback(p_callback.is_valid() ? p_callback : Callable());
}

void JoltPhysicsServer3D::_area_set_area_monitor_callback(
	const RID& p_area,
	const Callable& p_callback
) {
	JoltPhysicsArea3D* area = area_owner.get_or_null(p_area);
	ERR_FAIL_NULL(area);

	area->set_area_monitor_callback(p_callback.is_valid() ? p_callback : Callable());
}

void JoltPhysicsServer3D::_area_set_ray_pickable(
	[[maybe_unused]] const RID& p_area,
	[[maybe_unused]] bool p_enable
) {
	ERR_FAIL_NOT_IMPL();
}

RID JoltPhysicsServer3D::_body_create() {
	JoltPhysicsBody3D* body = memnew(JoltPhysicsBody3D);
	RID rid = body_owner.make_rid(body);
	body->set_rid(rid);
	return rid;
}

void JoltPhysicsServer3D::_body_set_space(const RID& p_body, const RID& p_space) {
	JoltPhysicsBody3D* body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);

	JoltPhysicsSpace3D* space = nullptr;

	if (p_space.is_valid()) {
		space = space_owner.get_or_null(p_space);
		ERR_FAIL_NULL(space);
	}

	body->set_space(space);
}

RID JoltPhysicsServer3D::_body_get_space([[maybe_unused]] const RID& p_body) const {
	ERR_FAIL_D_NOT_IMPL();
}

void JoltPhysicsServer3D::_body_set_mode(const RID& p_body, BodyMode p_mode) {
	JoltPhysicsBody3D* body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);

	body->set_mode(p_mode);
}

PhysicsServer3D::BodyMode JoltPhysicsServer3D::_body_get_mode([[maybe_unused]] const RID& p_body
) const {
	ERR_FAIL_D_NOT_IMPL();
}

void JoltPhysicsServer3D::_body_add_shape(
	const RID& p_body,
	const RID& p_shape,
	const Transform3D& p_transform,
	bool p_disabled
) {
	JoltPhysicsBody3D* body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);

	JoltPhysicsShape3D* shape = shape_owner.get_or_null(p_shape);
	ERR_FAIL_NULL(shape);

	body->add_shape(shape, p_transform, p_disabled);
}

void JoltPhysicsServer3D::_body_set_shape(
	[[maybe_unused]] const RID& p_body,
	[[maybe_unused]] int64_t p_shape_idx,
	[[maybe_unused]] const RID& p_shape
) {
	ERR_FAIL_NOT_IMPL();
}

void JoltPhysicsServer3D::_body_set_shape_transform(
	const RID& p_body,
	int64_t p_shape_idx,
	const Transform3D& p_transform
) {
	JoltPhysicsBody3D* body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);

	body->set_shape_transform(p_shape_idx, p_transform);
}

int64_t JoltPhysicsServer3D::_body_get_shape_count([[maybe_unused]] const RID& p_body) const {
	ERR_FAIL_D_NOT_IMPL();
}

RID JoltPhysicsServer3D::_body_get_shape(
	[[maybe_unused]] const RID& p_body,
	[[maybe_unused]] int64_t p_shape_idx
) const {
	ERR_FAIL_D_NOT_IMPL();
}

Transform3D JoltPhysicsServer3D::_body_get_shape_transform(
	[[maybe_unused]] const RID& p_body,
	[[maybe_unused]] int64_t p_shape_idx
) const {
	ERR_FAIL_D_NOT_IMPL();
}

void JoltPhysicsServer3D::_body_remove_shape(const RID& p_body, int64_t p_shape_idx) {
	JoltPhysicsBody3D* body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);

	body->remove_shape((int)p_shape_idx);
}

void JoltPhysicsServer3D::_body_clear_shapes([[maybe_unused]] const RID& p_body) {
	ERR_FAIL_NOT_IMPL();
}

void JoltPhysicsServer3D::_body_set_shape_disabled(
	[[maybe_unused]] const RID& p_body,
	[[maybe_unused]] int64_t p_shape_idx,
	[[maybe_unused]] bool p_disabled
) {
	ERR_FAIL_NOT_IMPL();
}

void JoltPhysicsServer3D::_body_attach_object_instance_id(const RID& p_body, int64_t p_id) {
	JoltPhysicsBody3D* body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);

	body->set_instance_id(p_id);
}

int64_t JoltPhysicsServer3D::_body_get_object_instance_id(const RID& p_body) const {
	JoltPhysicsBody3D* body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL_V(body, -1);

	return body->get_instance_id();
}

void JoltPhysicsServer3D::_body_set_enable_continuous_collision_detection(
	const RID& p_body,
	bool p_enable
) {
	JoltPhysicsBody3D* body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);

	body->set_ccd_enabled(p_enable);
}

bool JoltPhysicsServer3D::_body_is_continuous_collision_detection_enabled(const RID& p_body) const {
	JoltPhysicsBody3D* body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL_D(body);

	return body->is_ccd_enabled();
}

void JoltPhysicsServer3D::_body_set_collision_layer(const RID& p_body, int64_t p_layer) {
	JoltPhysicsBody3D* body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);

	body->set_collision_layer((uint32_t)p_layer);
}

int64_t JoltPhysicsServer3D::_body_get_collision_layer(const RID& p_body) const {
	JoltPhysicsBody3D* body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL_D(body);

	return body->get_collision_layer();
}

void JoltPhysicsServer3D::_body_set_collision_mask(const RID& p_body, int64_t p_mask) {
	JoltPhysicsBody3D* body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);

	body->set_collision_mask((uint32_t)p_mask);
}

int64_t JoltPhysicsServer3D::_body_get_collision_mask(const RID& p_body) const {
	JoltPhysicsBody3D* body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL_D(body);

	return body->get_collision_mask();
}

void JoltPhysicsServer3D::_body_set_collision_priority(
	[[maybe_unused]] const RID& p_body,
	double p_priority
) {
	if (p_priority != 1.0) {
		WARN_PRINT(
			"Collision priority is not supported by Godot Jolt. "
			"Any value will be treated as a value of 1."
		);
	}
}

double JoltPhysicsServer3D::_body_get_collision_priority([[maybe_unused]] const RID& p_body) const {
	return 1.0;
}

void JoltPhysicsServer3D::_body_set_user_flags(
	[[maybe_unused]] const RID& p_body,
	[[maybe_unused]] int64_t p_flags
) {
	ERR_FAIL_NOT_IMPL();
}

int64_t JoltPhysicsServer3D::_body_get_user_flags([[maybe_unused]] const RID& p_body) const {
	ERR_FAIL_D_NOT_IMPL();
}

void JoltPhysicsServer3D::_body_set_param(
	const RID& p_body,
	BodyParameter p_param,
	const Variant& p_value
) {
	JoltPhysicsBody3D* body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);

	body->set_param(p_param, p_value);
}

Variant JoltPhysicsServer3D::_body_get_param(const RID& p_body, BodyParameter p_param) const {
	JoltPhysicsBody3D* body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL_D(body);

	return body->get_param(p_param);
}

void JoltPhysicsServer3D::_body_reset_mass_properties([[maybe_unused]] const RID& p_body) {
	ERR_FAIL_NOT_IMPL();
}

void JoltPhysicsServer3D::_body_set_state(
	const RID& p_body,
	BodyState p_state,
	const Variant& p_value
) {
	JoltPhysicsBody3D* body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);

	body->set_state(p_state, p_value);
}

Variant JoltPhysicsServer3D::_body_get_state(const RID& p_body, BodyState p_state) const {
	JoltPhysicsBody3D* body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL_D(body);

	return body->get_state(p_state);
}

void JoltPhysicsServer3D::_body_apply_central_impulse(
	[[maybe_unused]] const RID& p_body,
	[[maybe_unused]] const Vector3& p_impulse
) {
	ERR_FAIL_NOT_IMPL();
}

void JoltPhysicsServer3D::_body_apply_impulse(
	[[maybe_unused]] const RID& p_body,
	[[maybe_unused]] const Vector3& p_impulse,
	[[maybe_unused]] const Vector3& p_position
) {
	ERR_FAIL_NOT_IMPL();
}

void JoltPhysicsServer3D::_body_apply_torque_impulse(
	[[maybe_unused]] const RID& p_body,
	[[maybe_unused]] const Vector3& p_impulse
) {
	ERR_FAIL_NOT_IMPL();
}

void JoltPhysicsServer3D::_body_apply_central_force(
	[[maybe_unused]] const RID& p_body,
	[[maybe_unused]] const Vector3& p_force
) {
	ERR_FAIL_NOT_IMPL();
}

void JoltPhysicsServer3D::_body_apply_force(
	[[maybe_unused]] const RID& p_body,
	[[maybe_unused]] const Vector3& p_force,
	[[maybe_unused]] const Vector3& p_position
) {
	ERR_FAIL_NOT_IMPL();
}

void JoltPhysicsServer3D::_body_apply_torque(
	[[maybe_unused]] const RID& p_body,
	[[maybe_unused]] const Vector3& p_torque
) {
	ERR_FAIL_NOT_IMPL();
}

void JoltPhysicsServer3D::_body_add_constant_central_force(
	const RID& p_body,
	const Vector3& p_force
) {
	JoltPhysicsBody3D* body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);

	body->add_constant_central_force(p_force);
}

void JoltPhysicsServer3D::_body_add_constant_force(
	const RID& p_body,
	const Vector3& p_force,
	const Vector3& p_position
) {
	JoltPhysicsBody3D* body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);

	body->add_constant_force(p_force, p_position);
}

void JoltPhysicsServer3D::_body_add_constant_torque(const RID& p_body, const Vector3& p_torque) {
	JoltPhysicsBody3D* body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);

	body->add_constant_torque(p_torque);
}

void JoltPhysicsServer3D::_body_set_constant_force(const RID& p_body, const Vector3& p_force) {
	JoltPhysicsBody3D* body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);

	body->set_constant_force(p_force);
}

Vector3 JoltPhysicsServer3D::_body_get_constant_force(const RID& p_body) const {
	JoltPhysicsBody3D* body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL_D(body);

	return body->get_constant_force();
}

void JoltPhysicsServer3D::_body_set_constant_torque(const RID& p_body, const Vector3& p_torque) {
	JoltPhysicsBody3D* body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);

	body->set_constant_torque(p_torque);
}

Vector3 JoltPhysicsServer3D::_body_get_constant_torque(const RID& p_body) const {
	JoltPhysicsBody3D* body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL_D(body);

	return body->get_constant_torque();
}

void JoltPhysicsServer3D::_body_set_axis_velocity(
	[[maybe_unused]] const RID& p_body,
	[[maybe_unused]] const Vector3& p_axis_velocity
) {
	ERR_FAIL_NOT_IMPL();
}

void JoltPhysicsServer3D::_body_set_axis_lock(
	[[maybe_unused]] const RID& p_body,
	[[maybe_unused]] BodyAxis p_axis,
	bool p_lock
) {
	if (p_lock) {
		WARN_PRINT(
			"Axis lock is not supported by Godot Jolt. "
			"Any such setting will be treated as disabled."
		);
	}
}

bool JoltPhysicsServer3D::_body_is_axis_locked(
	[[maybe_unused]] const RID& p_body,
	[[maybe_unused]] BodyAxis p_axis
) const {
	ERR_FAIL_D_NOT_IMPL();
}

void JoltPhysicsServer3D::_body_add_collision_exception(
	[[maybe_unused]] const RID& p_body,
	[[maybe_unused]] const RID& p_excepted_body
) {
	ERR_FAIL_NOT_IMPL();
}

void JoltPhysicsServer3D::_body_remove_collision_exception(
	[[maybe_unused]] const RID& p_body,
	[[maybe_unused]] const RID& p_excepted_body
) {
	ERR_FAIL_NOT_IMPL();
}

TypedArray<RID> JoltPhysicsServer3D::_body_get_collision_exceptions(
	[[maybe_unused]] const RID& p_body
) const {
	ERR_FAIL_D_NOT_IMPL();
}

void JoltPhysicsServer3D::_body_set_max_contacts_reported(
	[[maybe_unused]] const RID& p_body,
	[[maybe_unused]] int64_t p_amount
) {
	ERR_FAIL_NOT_IMPL();
}

int64_t JoltPhysicsServer3D::_body_get_max_contacts_reported([[maybe_unused]] const RID& p_body
) const {
	ERR_FAIL_D_NOT_IMPL();
}

void JoltPhysicsServer3D::_body_set_contacts_reported_depth_threshold(
	[[maybe_unused]] const RID& p_body,
	[[maybe_unused]] double p_threshold
) {
	ERR_FAIL_NOT_IMPL();
}

double JoltPhysicsServer3D::_body_get_contacts_reported_depth_threshold(
	[[maybe_unused]] const RID& p_body
) const {
	ERR_FAIL_D_NOT_IMPL();
}

void JoltPhysicsServer3D::_body_set_omit_force_integration(
	[[maybe_unused]] const RID& p_body,
	[[maybe_unused]] bool p_enable
) {
	ERR_FAIL_NOT_IMPL();
}

bool JoltPhysicsServer3D::_body_is_omitting_force_integration([[maybe_unused]] const RID& p_body
) const {
	ERR_FAIL_D_NOT_IMPL();
}

void JoltPhysicsServer3D::_body_set_state_sync_callback(
	const RID& p_body,
	const Callable& p_callable
) {
	JoltPhysicsBody3D* body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);

	body->set_state_sync_callback(p_callable);
}

void JoltPhysicsServer3D::_body_set_force_integration_callback(
	[[maybe_unused]] const RID& p_body,
	[[maybe_unused]] const Callable& p_callable,
	[[maybe_unused]] const Variant& p_userdata
) {
	ERR_FAIL_NOT_IMPL();
}

void JoltPhysicsServer3D::_body_set_ray_pickable(const RID& p_body, bool p_enable) {
	JoltPhysicsBody3D* body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);

	body->set_ray_pickable(p_enable);
}

bool JoltPhysicsServer3D::_body_test_motion(
	[[maybe_unused]] const RID& p_body,
	[[maybe_unused]] const Transform3D& p_from,
	[[maybe_unused]] const Vector3& p_motion,
	[[maybe_unused]] double p_margin,
	[[maybe_unused]] int64_t p_max_collisions,
	[[maybe_unused]] bool p_collide_separation_ray,
	[[maybe_unused]] PhysicsServer3DExtensionMotionResult* p_result
) const {
	ERR_FAIL_D_NOT_IMPL();
}

PhysicsDirectBodyState3D* JoltPhysicsServer3D::_body_get_direct_state(
	[[maybe_unused]] const RID& p_body
) {
	ERR_FAIL_D_NOT_IMPL();
}

RID JoltPhysicsServer3D::_soft_body_create() {
	ERR_FAIL_D_MSG("SoftBody3D is not supported by Godot Jolt.");
}

void JoltPhysicsServer3D::_soft_body_update_rendering_server(
	[[maybe_unused]] const RID& p_body,
	[[maybe_unused]] PhysicsServer3DRenderingServerHandler* p_rendering_server_handler
) {
	ERR_FAIL_MSG("SoftBody3D is not supported by Godot Jolt.");
}

void JoltPhysicsServer3D::_soft_body_set_space(
	[[maybe_unused]] const RID& p_body,
	[[maybe_unused]] const RID& p_space
) {
	ERR_FAIL_MSG("SoftBody3D is not supported by Godot Jolt.");
}

RID JoltPhysicsServer3D::_soft_body_get_space([[maybe_unused]] const RID& p_body) const {
	ERR_FAIL_D_MSG("SoftBody3D is not supported by Godot Jolt.");
}

void JoltPhysicsServer3D::_soft_body_set_mesh(
	[[maybe_unused]] const RID& p_body,
	[[maybe_unused]] const RID& p_mesh
) {
	ERR_FAIL_MSG("SoftBody3D is not supported by Godot Jolt.");
}

AABB JoltPhysicsServer3D::_soft_body_get_bounds([[maybe_unused]] const RID& p_body) const {
	ERR_FAIL_D_MSG("SoftBody3D is not supported by Godot Jolt.");
}

void JoltPhysicsServer3D::_soft_body_set_collision_layer(
	[[maybe_unused]] const RID& p_body,
	[[maybe_unused]] int64_t p_layer
) {
	ERR_FAIL_MSG("SoftBody3D is not supported by Godot Jolt.");
}

int64_t JoltPhysicsServer3D::_soft_body_get_collision_layer([[maybe_unused]] const RID& p_body
) const {
	ERR_FAIL_D_MSG("SoftBody3D is not supported by Godot Jolt.");
}

void JoltPhysicsServer3D::_soft_body_set_collision_mask(
	[[maybe_unused]] const RID& p_body,
	[[maybe_unused]] int64_t p_mask
) {
	ERR_FAIL_MSG("SoftBody3D is not supported by Godot Jolt.");
}

int64_t JoltPhysicsServer3D::_soft_body_get_collision_mask([[maybe_unused]] const RID& p_body
) const {
	ERR_FAIL_D_MSG("SoftBody3D is not supported by Godot Jolt.");
}

void JoltPhysicsServer3D::_soft_body_add_collision_exception(
	[[maybe_unused]] const RID& p_body,
	[[maybe_unused]] const RID& p_body_b
) {
	ERR_FAIL_MSG("SoftBody3D is not supported by Godot Jolt.");
}

void JoltPhysicsServer3D::_soft_body_remove_collision_exception(
	[[maybe_unused]] const RID& p_body,
	[[maybe_unused]] const RID& p_body_b
) {
	ERR_FAIL_MSG("SoftBody3D is not supported by Godot Jolt.");
}

TypedArray<RID> JoltPhysicsServer3D::_soft_body_get_collision_exceptions(
	[[maybe_unused]] const RID& p_body
) const {
	ERR_FAIL_D_MSG("SoftBody3D is not supported by Godot Jolt.");
}

void JoltPhysicsServer3D::_soft_body_set_state(
	[[maybe_unused]] const RID& p_body,
	[[maybe_unused]] BodyState p_state,
	[[maybe_unused]] const Variant& p_variant
) {
	ERR_FAIL_MSG("SoftBody3D is not supported by Godot Jolt.");
}

Variant JoltPhysicsServer3D::_soft_body_get_state(
	[[maybe_unused]] const RID& p_body,
	[[maybe_unused]] BodyState p_state
) const {
	ERR_FAIL_D_MSG("SoftBody3D is not supported by Godot Jolt.");
}

void JoltPhysicsServer3D::_soft_body_set_transform(
	[[maybe_unused]] const RID& p_body,
	[[maybe_unused]] const Transform3D& p_transform
) {
	ERR_FAIL_MSG("SoftBody3D is not supported by Godot Jolt.");
}

void JoltPhysicsServer3D::_soft_body_set_ray_pickable(
	[[maybe_unused]] const RID& p_body,
	[[maybe_unused]] bool p_enable
) {
	ERR_FAIL_MSG("SoftBody3D is not supported by Godot Jolt.");
}

void JoltPhysicsServer3D::_soft_body_set_simulation_precision(
	[[maybe_unused]] const RID& p_body,
	[[maybe_unused]] int64_t p_simulation_precision
) {
	ERR_FAIL_MSG("SoftBody3D is not supported by Godot Jolt.");
}

int64_t JoltPhysicsServer3D::_soft_body_get_simulation_precision([[maybe_unused]] const RID& p_body
) const {
	ERR_FAIL_D_MSG("SoftBody3D is not supported by Godot Jolt.");
}

void JoltPhysicsServer3D::_soft_body_set_total_mass(
	[[maybe_unused]] const RID& p_body,
	[[maybe_unused]] double p_total_mass
) {
	ERR_FAIL_MSG("SoftBody3D is not supported by Godot Jolt.");
}

double JoltPhysicsServer3D::_soft_body_get_total_mass([[maybe_unused]] const RID& p_body) const {
	ERR_FAIL_D_MSG("SoftBody3D is not supported by Godot Jolt.");
}

void JoltPhysicsServer3D::_soft_body_set_linear_stiffness(
	[[maybe_unused]] const RID& p_body,
	[[maybe_unused]] double p_linear_stiffness
) {
	ERR_FAIL_MSG("SoftBody3D is not supported by Godot Jolt.");
}

double JoltPhysicsServer3D::_soft_body_get_linear_stiffness([[maybe_unused]] const RID& p_body
) const {
	ERR_FAIL_D_MSG("SoftBody3D is not supported by Godot Jolt.");
}

void JoltPhysicsServer3D::_soft_body_set_pressure_coefficient(
	[[maybe_unused]] const RID& p_body,
	[[maybe_unused]] double p_pressure_coefficient
) {
	ERR_FAIL_MSG("SoftBody3D is not supported by Godot Jolt.");
}

double JoltPhysicsServer3D::_soft_body_get_pressure_coefficient([[maybe_unused]] const RID& p_body
) const {
	ERR_FAIL_D_MSG("SoftBody3D is not supported by Godot Jolt.");
}

void JoltPhysicsServer3D::_soft_body_set_damping_coefficient(
	[[maybe_unused]] const RID& p_body,
	[[maybe_unused]] double p_damping_coefficient
) {
	ERR_FAIL_MSG("SoftBody3D is not supported by Godot Jolt.");
}

double JoltPhysicsServer3D::_soft_body_get_damping_coefficient([[maybe_unused]] const RID& p_body
) const {
	ERR_FAIL_D_MSG("SoftBody3D is not supported by Godot Jolt.");
}

void JoltPhysicsServer3D::_soft_body_set_drag_coefficient(
	[[maybe_unused]] const RID& p_body,
	[[maybe_unused]] double p_drag_coefficient
) {
	ERR_FAIL_MSG("SoftBody3D is not supported by Godot Jolt.");
}

double JoltPhysicsServer3D::_soft_body_get_drag_coefficient([[maybe_unused]] const RID& p_body
) const {
	ERR_FAIL_D_MSG("SoftBody3D is not supported by Godot Jolt.");
}

void JoltPhysicsServer3D::_soft_body_move_point(
	[[maybe_unused]] const RID& p_body,
	[[maybe_unused]] int64_t p_point_index,
	[[maybe_unused]] const Vector3& p_global_position
) {
	ERR_FAIL_MSG("SoftBody3D is not supported by Godot Jolt.");
}

Vector3 JoltPhysicsServer3D::_soft_body_get_point_global_position(
	[[maybe_unused]] const RID& p_body,
	[[maybe_unused]] int64_t p_point_index
) const {
	ERR_FAIL_D_MSG("SoftBody3D is not supported by Godot Jolt.");
}

void JoltPhysicsServer3D::_soft_body_remove_all_pinned_points([[maybe_unused]] const RID& p_body) {
	ERR_FAIL_MSG("SoftBody3D is not supported by Godot Jolt.");
}

void JoltPhysicsServer3D::_soft_body_pin_point(
	[[maybe_unused]] const RID& p_body,
	[[maybe_unused]] int64_t p_point_index,
	[[maybe_unused]] bool p_pin
) {
	ERR_FAIL_MSG("SoftBody3D is not supported by Godot Jolt.");
}

bool JoltPhysicsServer3D::_soft_body_is_point_pinned(
	[[maybe_unused]] const RID& p_body,
	[[maybe_unused]] int64_t p_point_index
) const {
	ERR_FAIL_D_MSG("SoftBody3D is not supported by Godot Jolt.");
}

RID JoltPhysicsServer3D::_joint_create() {
	ERR_FAIL_D_NOT_IMPL();
}

void JoltPhysicsServer3D::_joint_clear([[maybe_unused]] const RID& p_joint) {
	ERR_FAIL_NOT_IMPL();
}

void JoltPhysicsServer3D::_joint_make_pin(
	[[maybe_unused]] const RID& p_joint,
	[[maybe_unused]] const RID& p_body_a,
	[[maybe_unused]] const Vector3& p_local_a,
	[[maybe_unused]] const RID& p_body_b,
	[[maybe_unused]] const Vector3& p_local_b
) {
	ERR_FAIL_NOT_IMPL();
}

void JoltPhysicsServer3D::_pin_joint_set_param(
	[[maybe_unused]] const RID& p_joint,
	[[maybe_unused]] PinJointParam p_param,
	[[maybe_unused]] double p_value
) {
	ERR_FAIL_NOT_IMPL();
}

double JoltPhysicsServer3D::_pin_joint_get_param(
	[[maybe_unused]] const RID& p_joint,
	[[maybe_unused]] PinJointParam p_param
) const {
	ERR_FAIL_D_NOT_IMPL();
}

void JoltPhysicsServer3D::_pin_joint_set_local_a(
	[[maybe_unused]] const RID& p_joint,
	[[maybe_unused]] const Vector3& p_local_a
) {
	ERR_FAIL_NOT_IMPL();
}

Vector3 JoltPhysicsServer3D::_pin_joint_get_local_a([[maybe_unused]] const RID& p_joint) const {
	ERR_FAIL_D_NOT_IMPL();
}

void JoltPhysicsServer3D::_pin_joint_set_local_b(
	[[maybe_unused]] const RID& p_joint,
	[[maybe_unused]] const Vector3& p_local_b
) {
	ERR_FAIL_NOT_IMPL();
}

Vector3 JoltPhysicsServer3D::_pin_joint_get_local_b([[maybe_unused]] const RID& p_joint) const {
	ERR_FAIL_D_NOT_IMPL();
}

void JoltPhysicsServer3D::_joint_make_hinge(
	[[maybe_unused]] const RID& p_joint,
	[[maybe_unused]] const RID& p_body_a,
	[[maybe_unused]] const Transform3D& p_hinge_a,
	[[maybe_unused]] const RID& p_body_b,
	[[maybe_unused]] const Transform3D& p_hinge_b
) {
	ERR_FAIL_NOT_IMPL();
}

void JoltPhysicsServer3D::_joint_make_hinge_simple(
	[[maybe_unused]] const RID& p_joint,
	[[maybe_unused]] const RID& p_body_a,
	[[maybe_unused]] const Vector3& p_pivot_a,
	[[maybe_unused]] const Vector3& p_axis_a,
	[[maybe_unused]] const RID& p_body_b,
	[[maybe_unused]] const Vector3& p_pivot_b,
	[[maybe_unused]] const Vector3& p_axis_b
) {
	ERR_FAIL_NOT_IMPL();
}

void JoltPhysicsServer3D::_hinge_joint_set_param(
	[[maybe_unused]] const RID& p_joint,
	[[maybe_unused]] HingeJointParam p_param,
	[[maybe_unused]] double p_value
) {
	ERR_FAIL_NOT_IMPL();
}

double JoltPhysicsServer3D::_hinge_joint_get_param(
	[[maybe_unused]] const RID& p_joint,
	[[maybe_unused]] HingeJointParam p_param
) const {
	ERR_FAIL_D_NOT_IMPL();
}

void JoltPhysicsServer3D::_hinge_joint_set_flag(
	[[maybe_unused]] const RID& p_joint,
	[[maybe_unused]] HingeJointFlag p_flag,
	[[maybe_unused]] bool p_enabled
) {
	ERR_FAIL_NOT_IMPL();
}

bool JoltPhysicsServer3D::_hinge_joint_get_flag(
	[[maybe_unused]] const RID& p_joint,
	[[maybe_unused]] HingeJointFlag p_flag
) const {
	ERR_FAIL_D_NOT_IMPL();
}

void JoltPhysicsServer3D::_joint_make_slider(
	[[maybe_unused]] const RID& p_joint,
	[[maybe_unused]] const RID& p_body_a,
	[[maybe_unused]] const Transform3D& p_local_ref_a,
	[[maybe_unused]] const RID& p_body_b,
	[[maybe_unused]] const Transform3D& p_local_ref_b
) {
	ERR_FAIL_NOT_IMPL();
}

void JoltPhysicsServer3D::_slider_joint_set_param(
	[[maybe_unused]] const RID& p_joint,
	[[maybe_unused]] SliderJointParam p_param,
	[[maybe_unused]] double p_value
) {
	ERR_FAIL_NOT_IMPL();
}

double JoltPhysicsServer3D::_slider_joint_get_param(
	[[maybe_unused]] const RID& p_joint,
	[[maybe_unused]] SliderJointParam p_param
) const {
	ERR_FAIL_D_NOT_IMPL();
}

void JoltPhysicsServer3D::_joint_make_cone_twist(
	[[maybe_unused]] const RID& p_joint,
	[[maybe_unused]] const RID& p_body_a,
	[[maybe_unused]] const Transform3D& p_local_ref_a,
	[[maybe_unused]] const RID& p_body_b,
	[[maybe_unused]] const Transform3D& p_local_ref_b
) {
	ERR_FAIL_NOT_IMPL();
}

void JoltPhysicsServer3D::_cone_twist_joint_set_param(
	[[maybe_unused]] const RID& p_joint,
	[[maybe_unused]] ConeTwistJointParam p_param,
	[[maybe_unused]] double p_value
) {
	ERR_FAIL_NOT_IMPL();
}

double JoltPhysicsServer3D::_cone_twist_joint_get_param(
	[[maybe_unused]] const RID& p_joint,
	[[maybe_unused]] ConeTwistJointParam p_param
) const {
	ERR_FAIL_D_NOT_IMPL();
}

void JoltPhysicsServer3D::_joint_make_generic_6dof(
	[[maybe_unused]] const RID& p_joint,
	[[maybe_unused]] const RID& p_body_a,
	[[maybe_unused]] const Transform3D& p_local_ref_a,
	[[maybe_unused]] const RID& p_body_b,
	[[maybe_unused]] const Transform3D& p_local_ref_b
) {
	ERR_FAIL_NOT_IMPL();
}

void JoltPhysicsServer3D::_generic_6dof_joint_set_param(
	[[maybe_unused]] const RID& p_joint,
	[[maybe_unused]] Vector3::Axis p_axis,
	[[maybe_unused]] PhysicsServer3D::G6DOFJointAxisParam p_param,
	[[maybe_unused]] double p_value
) {
	ERR_FAIL_NOT_IMPL();
}

double JoltPhysicsServer3D::_generic_6dof_joint_get_param(
	[[maybe_unused]] const RID& p_joint,
	[[maybe_unused]] Vector3::Axis p_axis,
	[[maybe_unused]] PhysicsServer3D::G6DOFJointAxisParam p_param
) const {
	ERR_FAIL_D_NOT_IMPL();
}

void JoltPhysicsServer3D::_generic_6dof_joint_set_flag(
	[[maybe_unused]] const RID& p_joint,
	[[maybe_unused]] Vector3::Axis p_axis,
	[[maybe_unused]] PhysicsServer3D::G6DOFJointAxisFlag p_flag,
	[[maybe_unused]] bool p_enable
) {
	ERR_FAIL_NOT_IMPL();
}

bool JoltPhysicsServer3D::_generic_6dof_joint_get_flag(
	[[maybe_unused]] const RID& p_joint,
	[[maybe_unused]] Vector3::Axis p_axis,
	[[maybe_unused]] PhysicsServer3D::G6DOFJointAxisFlag p_flag
) const {
	ERR_FAIL_D_NOT_IMPL();
}

PhysicsServer3D::JointType JoltPhysicsServer3D::_joint_get_type([[maybe_unused]] const RID& p_joint
) const {
	ERR_FAIL_D_NOT_IMPL();
}

void JoltPhysicsServer3D::_joint_set_solver_priority(
	[[maybe_unused]] const RID& p_joint,
	[[maybe_unused]] int64_t p_priority
) {
	ERR_FAIL_NOT_IMPL();
}

int64_t JoltPhysicsServer3D::_joint_get_solver_priority([[maybe_unused]] const RID& p_joint) const {
	ERR_FAIL_D_NOT_IMPL();
}

void JoltPhysicsServer3D::_free_rid(const RID& p_rid) {
	if (shape_owner.owns(p_rid)) {
		JoltPhysicsShape3D* shape = shape_owner.get_or_null(p_rid);

		if (JoltPhysicsCollisionObject3D* owner = shape->get_owner()) {
			owner->remove_shape(shape);
		}

		shape_owner.free(p_rid);
		memdelete(shape);
	} else if (body_owner.owns(p_rid)) {
		JoltPhysicsBody3D* body = body_owner.get_or_null(p_rid);

		body->set_space(nullptr);

		while (body->get_shape_count() > 0) {
			body->remove_shape(0);
		}

		body_owner.free(p_rid);
		memdelete(body);
	} else if (area_owner.owns(p_rid)) {
		JoltPhysicsArea3D* area = area_owner.get_or_null(p_rid);
		area->set_space(nullptr);
		area_owner.free(p_rid);
		memdelete(area);
	} else if (space_owner.owns(p_rid)) {
		JoltPhysicsSpace3D* space = space_owner.get_or_null(p_rid);
		space_set_active(p_rid, false);
		space_owner.free(p_rid);
		memdelete(space);
	} else {
		ERR_FAIL_MSG("Invalid ID.");
	}
}

void JoltPhysicsServer3D::_set_active(bool p_active) {
	active = p_active;
}

void JoltPhysicsServer3D::_init() {
	if (server_count++ == 0) {
		init_statics();
	}
}

void JoltPhysicsServer3D::_step(double p_step) {
	if (!active) {
		return;
	}

	for (JoltPhysicsSpace3D* active_space : active_spaces) {
		active_space->step((float)p_step);
	}
}

void JoltPhysicsServer3D::_sync() {
	doing_sync = true;
}

void JoltPhysicsServer3D::_flush_queries() {
	if (!active) {
		return;
	}

	flushing_queries = true;

	for (JoltPhysicsSpace3D* space : active_spaces) {
		space->call_queries();
	}

	flushing_queries = false;
}

void JoltPhysicsServer3D::_end_sync() {
	doing_sync = false;
}

void JoltPhysicsServer3D::_finish() {
	if (job_system != nullptr) {
		delete job_system;
		job_system = nullptr;
	}

	if (--server_count == 0) {
		finish_statics();
	}
}

bool JoltPhysicsServer3D::_is_flushing_queries() const {
	return flushing_queries;
}

int64_t JoltPhysicsServer3D::_get_process_info([[maybe_unused]] ProcessInfo p_process_info) {
	return 0;
}
