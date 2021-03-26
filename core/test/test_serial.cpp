#include <moveit/task_constructor/task.h>
#include <moveit/task_constructor/stages.h>
#include <moveit/task_constructor/cost_terms.h>
#include <moveit/task_constructor/solvers/joint_interpolation.h>
#include <moveit/planning_scene/planning_scene.h>
#include <moveit/utils/robot_model_test_utils.h>

#include "models.h"
#include <list>
#include <memory>
#include <gtest/gtest.h>

using namespace moveit::task_constructor;
using namespace planning_scene;

MOVEIT_STRUCT_FORWARD(PredefinedCosts);
struct PredefinedCosts : CostTerm
{
	mutable std::list<double> costs_;  // list of costs to assign
	mutable double cost_ = 0.0;  // last assigned cost
	bool finite_;  // finite number of compute() attempts?

	PredefinedCosts(bool finite, std::list<double>&& costs) : costs_(std::move(costs)), finite_(finite) {}
	bool exhausted() const { return finite_ && costs_.empty(); }
	double cost() const {
		if (!costs_.empty()) {
			cost_ = costs_.front();
			costs_.pop_front();
		}
		return cost_;
	}

	double operator()(const SubTrajectory& s, std::string& comment) const override { return cost(); }
	double operator()(const SolutionSequence& s, std::string& comment) const override { return cost(); }
	double operator()(const WrappedSolution& s, std::string& comment) const override { return cost(); }
};

/** Generator creating solutions with given costs */
struct GeneratorMockup : Generator
{
	PlanningScenePtr ps_;
	PredefinedCosts costs_;
	static unsigned int id_;

	GeneratorMockup(std::initializer_list<double> costs = { 0.0 })
	  : Generator("GEN" + std::to_string(++id_)), costs_(true, costs) {}

	void init(const moveit::core::RobotModelConstPtr& robot_model) override {
		ps_.reset(new PlanningScene(robot_model));
		Generator::init(robot_model);
	}

	bool canCompute() const override { return !costs_.exhausted(); }
	void compute() override { spawn(InterfaceState(ps_), costs_.cost()); }
};

struct PropagatorMockup : public PropagatingEitherWay
{
	PredefinedCosts costs_;
	std::size_t solutions_per_compute_;

	unsigned int calls_ = 0;

	PropagatorMockup(std::initializer_list<double> costs = { 0.0 }, std::size_t solutions_per_compute = 1)
	  : PropagatingEitherWay(), costs_(false, costs), solutions_per_compute_(solutions_per_compute) {}

	void computeForward(const InterfaceState& from) override {
		++calls_;
		for (std::size_t i = 0; i < solutions_per_compute_; ++i) {
			SubTrajectory solution(robot_trajectory::RobotTrajectoryConstPtr(), costs_.cost());
			sendForward(from, InterfaceState(from.scene()->diff()), std::move(solution));
		}
	}
	void computeBackward(const InterfaceState& to) override {
		++calls_;
		for (std::size_t i = 0; i < solutions_per_compute_; ++i) {
			SubTrajectory solution(robot_trajectory::RobotTrajectoryConstPtr(), costs_.cost());
			sendBackward(InterfaceState(to.scene()->diff()), to, std::move(solution));
		}
	}
};
struct ForwardMockup : public PropagatorMockup
{
	static unsigned int id_;

	ForwardMockup(std::initializer_list<double> costs = { 0.0 }, std::size_t solutions_per_compute = 1)
	  : PropagatorMockup(costs, solutions_per_compute) {
		restrictDirection(FORWARD);
		setName("FW" + std::to_string(++id_));
	}
};
struct BackwardMockup : public PropagatorMockup
{
	static unsigned int id_;

	BackwardMockup(std::initializer_list<double> costs = { 0.0 }) : PropagatorMockup(costs) {
		restrictDirection(BACKWARD);
		setName("BW" + std::to_string(++id_));
	}
};

/* Forward propagator, contributing no solutions at all */
struct ForwardDummy : PropagatingForward
{
	using PropagatingForward::PropagatingForward;
	void computeForward(const InterfaceState& /*from*/) override {}
};

/* Connect creating solutions with given costs */
struct Connect : stages::Connect
{
	PlanningScenePtr ps_;
	PredefinedCostsPtr costs_;
	unsigned int calls_ = 0;
	static unsigned int id_;

	static GroupPlannerVector getPlanners() {
		auto planner = std::make_shared<solvers::JointInterpolationPlanner>();
		return { { "group", planner }, { "eef_group", planner } };
	}

	Connect(std::initializer_list<double> costs = {}, bool enforce_sequential = false)
	  : stages::Connect("CON" + std::to_string(++id_), getPlanners()) {
		costs_ = std::make_shared<PredefinedCosts>(false, costs);
		setCostTerm(costs_);
		if (enforce_sequential)
			setProperty("merge_mode", SEQUENTIAL);
	}
	void compute(const InterfaceState& from, const InterfaceState& to) override {
		++calls_;
		stages::Connect::compute(from, to);
	}
};

constexpr double inf = std::numeric_limits<double>::infinity();
unsigned int GeneratorMockup::id_ = 0;
unsigned int ForwardMockup::id_ = 0;
unsigned int BackwardMockup::id_ = 0;
unsigned int Connect::id_ = 0;

void resetIds() {
	GeneratorMockup::id_ = 0;
	ForwardMockup::id_ = 0;
	BackwardMockup::id_ = 0;
	Connect::id_ = 0;
}

template <typename C, typename S>
auto add(C& container, S* stage) -> S* {
	Stage::pointer ptr{ stage };
	container.add(std::move(ptr));
	return stage;
}

// https://github.com/ros-planning/moveit_task_constructor/issues/182
TEST(ConnectConnect, SuccSucc) {
	resetIds();

	Task t;
	t.setRobotModel(getModel());
	t.add(Stage::pointer(new GeneratorMockup({ 1, 2, 3 })));
	t.add(Stage::pointer(new Connect()));
	t.add(Stage::pointer(new GeneratorMockup({ 10, 20 })));
	t.add(Stage::pointer(new Connect()));
	t.add(Stage::pointer(new GeneratorMockup()));

	EXPECT_TRUE(t.plan());
	ASSERT_EQ(t.solutions().size(), 3u * 2u);
	std::vector<double> expected_costs = { 11, 12, 13, 21, 22, 23 };
	auto expected_cost = expected_costs.begin();
	for (const auto& s : t.solutions()) {
		EXPECT_EQ(s->cost(), *expected_cost);
		++expected_cost;
	}
}

// https://github.com/ros-planning/moveit_task_constructor/issues/218
TEST(ConnectConnect, FailSucc) {
	resetIds();
	Task t;
	t.setRobotModel(getModel());
	t.add(Stage::pointer(new GeneratorMockup()));
	t.add(Stage::pointer(new Connect({ inf }, true)));
	t.add(Stage::pointer(new GeneratorMockup()));
	t.add(Stage::pointer(new Connect()));
	t.add(Stage::pointer(new GeneratorMockup()));
	t.add(Stage::pointer(new ForwardDummy()));

	EXPECT_FALSE(t.plan());
}

TEST(Pruning, PropagatorFailure) {
	resetIds();
	Task t;
	t.setRobotModel(getModel());
	BackwardMockup* b;
	t.add(Stage::pointer(b = new BackwardMockup()));
	t.add(Stage::pointer(new GeneratorMockup({ 0 })));
	t.add(Stage::pointer(new ForwardMockup({ inf })));

	t.plan();

	ASSERT_EQ(t.solutions().size(), 0);
	// ForwardMockup fails, so the backward stage should never compute
	EXPECT_EQ(b->calls_, 0);
}

TEST(Pruning, PruningMultiForward) {
	resetIds();
	Task t;
	t.setRobotModel(getModel());

	t.add(Stage::pointer(new BackwardMockup()));
	t.add(Stage::pointer(new BackwardMockup()));
	t.add(Stage::pointer(new GeneratorMockup()));
	// spawn two solutions for the only incoming state
	t.add(Stage::pointer(new ForwardMockup({ 0, 0 }, 2)));
	// fail to extend the second solution
	t.add(Stage::pointer(new ForwardMockup({ 0, inf })));

	t.plan();

	// the second (infeasible) solution in the last stage must not disable
	// the earlier partial solution just because they share stage solutions
	ASSERT_EQ(t.solutions().size(), 1);
	EXPECT_EQ((*t.solutions().begin())->cost(), 0u);
}

TEST(Pruning, ConnectConnectForward) {
	resetIds();
	Task t;
	t.setRobotModel(getModel());
	Connect *c1, *c2;
	t.add(Stage::pointer(new GeneratorMockup()));
	t.add(Stage::pointer(c1 = new Connect({ inf, 0 })));  // 1st attempt is a failure
	t.add(Stage::pointer(new GeneratorMockup({ 0, 10, 20 })));
	t.add(Stage::pointer(new ForwardMockup()));
	t.add(Stage::pointer(c2 = new Connect()));
	t.add(Stage::pointer(new GeneratorMockup({ 1, 2, 3 })));

	t.plan();

	ASSERT_EQ(t.solutions().size(), 3u * 2u);
	std::vector<double> expected_costs = { 11, 12, 13, 21, 22, 23 };
	auto expected_cost = expected_costs.begin();
	for (const auto& s : t.solutions()) {
		EXPECT_EQ(s->cost(), *expected_cost);
		++expected_cost;
	}
	EXPECT_EQ(c1->calls_, 3u);
	EXPECT_EQ(c2->calls_, 6u);
}

TEST(Pruning, ConnectConnectBackward) {
	resetIds();
	Task t;
	t.setRobotModel(getModel());
	Connect *c1, *c2;
	t.add(Stage::pointer(new GeneratorMockup({ 1, 2, 3 })));
	t.add(Stage::pointer(c1 = new Connect()));
	t.add(Stage::pointer(new BackwardMockup()));
	t.add(Stage::pointer(new GeneratorMockup({ 0, inf, 10, 20 })));  // 2nd is a dummy to postpone creation of 3rd
	t.add(Stage::pointer(c2 = new Connect({ inf, 0 })));  // 1st attempt is a failure
	t.add(Stage::pointer(new GeneratorMockup()));

	t.plan();

	ASSERT_EQ(t.solutions().size(), 3u * 2u);
	std::vector<double> expected_costs = { 11, 12, 13, 21, 22, 23 };
	auto expected_cost = expected_costs.begin();
	for (const auto& s : t.solutions()) {
		EXPECT_EQ(s->cost(), *expected_cost);
		++expected_cost;
	}
	EXPECT_EQ(c2->calls_, 3u);
	EXPECT_EQ(c1->calls_, 6u);
}

TEST(Pruning, PropagateInsideContainerBoundaries) {
	resetIds();
	Task t;
	t.setRobotModel(getModel());
	add(t, new BackwardMockup({ inf }));
	add(t, new GeneratorMockup({ 0 }));
	auto c{ std::make_unique<SerialContainer>() };
	auto con = add(*c, new Connect());
	add(*c, new GeneratorMockup({ 0 }));
	t.add(std::move(c));

	t.plan();

	// the failure in the backward stage (outside the container)
	// should prune the expected computation of con
	EXPECT_EQ(con->calls_, 0);
}

TEST(Pruning, DISABLED_PropagateOutsideContainerBoundaries) {
	resetIds();
	Task t;
	t.setRobotModel(getModel());
	auto back = add(t, new BackwardMockup());
	add(t, new BackwardMockup());
	add(t, new GeneratorMockup({ 0 }));
	auto c{ std::make_unique<SerialContainer>() };
	add(*c, new ForwardMockup({ inf }));
	add(*c, new ForwardMockup());
	t.add(std::move(c));

	t.plan();

	// the failure inside the container should prune computing of back
	EXPECT_EQ(back->calls_, 0);
}