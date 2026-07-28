#include "SExprTk.hpp"
#include <iostream>

int main() {
    sexprtk::SExprTk rt;
    auto source = sexprtk::Source::from_string(
        "(kernel (semantic (lua \"return cartable.root\")) (semantic (s7 '(+ 1 2))))");
    auto cartable = rt.parse(source);

    std::cout << "input:  " << sexprtk::Serializer::to_string(cartable.root) << '\n';

    sexprtk::LuaKernel lua;
    sexprtk::S7Kernel s7;
    sexprtk::LuaKernelSemanticizer lua_sem;
    sexprtk::S7KernelSemanticizer s7_sem;

    std::cout << "lua kernel:       " << rt.run(source, lua) << '\n';
    std::cout << "s7 kernel:        " << rt.run(source, s7) << '\n';
    std::cout << "lua semanticizer: " << rt.run(source, lua_sem) << '\n';
    std::cout << "s7 semanticizer:  " << rt.run(source, s7_sem) << '\n';

    std::cout << "s7 symbol present: " << std::boolalpha
              << sexprtk::Analyzer::has_symbol(cartable.root, "s7") << '\n';
}
