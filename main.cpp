#include <iostream>
#include <queue>
#include <set>
#include <typeindex>
#include <unordered_map>
#include <utility>

struct HookContext {
  virtual ~HookContext() = default;
};
typedef std::function<void(HookContext &)> HookFunc;

struct Hook {
  public:
  Hook(HookFunc function, int priority)
      : function{std::move(function)}, priority{priority} {}
  virtual ~Hook() = default;

  void Call(HookContext &ctx) const {
    function(ctx);
  }

  // Define the less-than operator for comparison
  bool operator<(const Hook &other) const {
    // Change the comparison criteria as needed
    return priority < other.priority;
  }

  private:
  HookFunc function;
  int priority;
};

template<typename TContext, typename = typename std::enable_if<std::is_base_of<HookContext, TContext>::value>::type>
class TemplatedHook : public Hook {
  typedef void (*HookFuncOf)(TContext &context);

  public:
  TemplatedHook(HookFuncOf f, int priority)
      : Hook(CallWrapper(f), priority) {}

  static HookFunc CallWrapper(HookFuncOf f) {
    std::function<void(HookContext &)> m_wrapper = [f](HookContext &context) {
      auto &ctx = dynamic_cast<TContext &>(context);
      f(ctx);
    };
    return m_wrapper;
  }
};

typedef std::set<Hook> HookQueue;
typedef std::unordered_map<std::type_index, HookQueue> HookMap;

class Hooks {
  public:
  template<class TContext>
  typename std::enable_if<std::is_base_of<HookContext, TContext>::value, void>::type
  Register(void (*func)(TContext &context), int priority = 100) {
    auto &hooks = m_hook_map[typeid(TContext)];
    hooks.insert(TemplatedHook<TContext>(func, priority));
  }

  template<class TContext>
  typename std::enable_if<std::is_base_of<HookContext, TContext>::value, void>::type
  Execute(TContext &context) {
    auto it = m_hook_map.find(typeid(TContext));
    if (it != m_hook_map.end()) {
      auto hooks = it->second;
      for (const auto &hook: hooks) {
        hook.Call(context);
      }
    }
  }

  private:
  HookMap m_hook_map;
};

class MyCtx1 : public HookContext {
  public:
  void AddNumber(int n) {
    m_numbers.emplace_back(n);
  }

  const std::vector<int> &GetNumbers() {
    return m_numbers;
  }

  private:
  std::vector<int> m_numbers;
};
class MyCtx2 : public HookContext {};

class MyAction {
  public:
  static void Execute(MyCtx1 &ctx) {
    std::cout << "Hello from action (MyCtx1)" << std::endl;
    for (auto n: ctx.GetNumbers()) {
      std::cout << n << std::endl;
    }
  }
  static void Execute(MyCtx2 &ctx) {
    std::cout << "Hello from action (MyCtx2)" << std::endl;
  }
};

int main() {
  MyCtx1 a;
  MyCtx2 b;
  MyAction action;
  Hooks hooks;

  hooks.Register<MyCtx1>([](auto &ctx) {
    std::cout << "Hello from 1 (MyCtx1) " << std::endl;
    ctx.AddNumber(5);
  },
                         20);

  hooks.Register<MyCtx1>([](auto &ctx) {
    std::cout << "Hello from 2 (MyCtx1) " << std::endl;
    ctx.AddNumber(29);
  },
                         0);

  hooks.Register<MyCtx1>([](auto &ctx) {
    std::cout << "Hello from 3 (MyCtx1) " << std::endl;
    ctx.AddNumber(1990);
  },
                         10);

  hooks.Register<MyCtx1>(MyAction::Execute);

  hooks.Register<MyCtx2>([](auto &ctx) {
    std::cout << "Hello from 1 (MyCtx2)" << std::endl;
  });

  hooks.Register<MyCtx2>(MyAction::Execute, 101);

  hooks.Execute(a);
  hooks.Execute(b);

  /*
    Output:
    Hello from 2 (MyCtx1)
    Hello from 3 (MyCtx1)
    Hello from 1 (MyCtx1)
    Hello from action (MyCtx1)
    29
    1990
    5
    Hello from 1 (MyCtx2)
    Hello from action (MyCtx2)
  */
  return 0;
}