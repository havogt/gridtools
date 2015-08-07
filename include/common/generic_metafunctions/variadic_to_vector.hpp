#pragma once
#include <boost/mpl/vector.hpp>
#include <boost/mpl/push_back.hpp>

namespace gridtools{

#ifdef CXX11_ENABLED
/**
 * @struct variadic_to_vector
 * metafunction that returns a mpl vector from a pack of variadic arguments
 */
template <typename ...Args> struct variadic_to_vector;

template <class T, typename ...Args>
struct variadic_to_vector<T, Args...>
{
    typedef typename boost::mpl::push_front<typename variadic_to_vector<Args...>::type, T>::type type;
};

template <class T>
struct variadic_to_vector<T>
{
    typedef boost::mpl::vector<T> type;
};

template <>
struct variadic_to_vector<>
{
    typedef boost::mpl::vector<> type;
};


template <typename Vector>
struct rest_of;

template <typename First, typename ... Rest>
struct rest_of<boost::mpl::vector<First, Rest...> > {
    typedef typename variadic_to_vector<Rest...>::type type;
};

#endif

}
