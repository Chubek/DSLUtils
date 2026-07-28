#include "SExprTk.hpp"
#include <iostream>

int main() {
    sexprtk::SExprTk rt;

    auto source = sexprtk::Source::from_string(
        "(exchange (id 1) (payload \"ok\") (meta (version 2)))");
    auto cartable = rt.parse(source);

    std::cout << "sexpr: " << sexprtk::Serializer::to_string(cartable.root) << '\n';
    std::cout << "json:  " << sexprtk::Serializer::to_json(cartable.root) << '\n';
    std::cout << "atoms: " << sexprtk::Analyzer::count_atoms(cartable.root) << '\n';
    std::cout << "lists: " << sexprtk::Analyzer::count_lists(cartable.root) << '\n';
}
