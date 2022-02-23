#include <iostream>
#include <iterator>
#include <vector>

template <std::random_access_iterator RAIter>
static void SendByIterator(
    const RAIter &cbegin,
    const RAIter &cend)
{
    for (auto iter = cbegin; iter != cend; ++iter)
    {
        std::cout << *iter << std::endl;
    }
}

int main()
{
    std::vector<int> vec{1, 2, 3, 4, 5, 6};
    SendByIterator(vec.cbegin(), vec.cend());
}