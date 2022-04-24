#pragma once

template <bool B, class T = void>
struct enable_if {};

template <class T>
struct enable_if<true, T> {
  typedef T type;
};

template <bool b, class T = void>
using enable_if_t = typename enable_if<b, T>::type;

template <class T, T v>
struct integral_constant {
  static constexpr T value = v;
  using value_type = T;
  using type = integral_constant;  // using injected-class-name
  constexpr operator value_type() const noexcept { return value; }
};

using true_type = integral_constant<bool, true>;
using false_type = integral_constant<bool, false>;

template <class T, class U>
struct is_same : false_type {};

template <class T>
struct is_same<T, T> : true_type {};

template <typename T>
int sgn(T val) {
  return (T(0) < val) - (val < T(0));
}

template <typename first_type, typename second_type>
struct pair {
  first_type first;
  second_type second;
};

template <typename first_type, typename second_type>
pair<first_type, second_type> make_pair(first_type first, second_type second);

template <typename first_type, typename second_type>
pair<first_type, second_type> make_pair(first_type first, second_type second) {
  return pair<first_type, second_type>{first, second};
}

template <typename T>
T toNumber(const String& str) {
  return static_cast<T>(str);
}

template <>
double toNumber<double>(const String& str) {
  return str.toDouble();
}

template <>
long toNumber<long>(const String& str) {
  return str.toInt();
}

/// command format: "<commmand> <param1 name><param1value> <param2 name><param2
/// value> ..."
template <typename T>
T getCommandParam(const String& str, const String& pattern,
                  T defaultResult = T()) {
  const int endOfCommandIndex = str.indexOf(' ');
  const int patternIndex = str.indexOf(pattern, endOfCommandIndex + 1);
  if (endOfCommandIndex == -1 || patternIndex == -1) {
    return defaultResult;
  }
  const int endOfPatternIndex =
      str.indexOf(' ', patternIndex);  // (unsigned)(-1 is ok)
  const double result = toNumber<T>(
      str.substring(patternIndex + pattern.length(), endOfPatternIndex));
  return result;
}