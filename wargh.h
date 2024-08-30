#pragma once

#include <algorithm>
#include <sstream>
#include <limits>
#include <string>
#include <vector>
#include <set>
#include <map>
#include <cassert>

namespace argh
{
   // Terminology:
   // A command line is composed of 2 types of args:
   // 1. Positional args, i.e. free standing values
   // 2. Options: args beginning with '-'. We identify two kinds:
   //    2.1: Flags: boolean options =>  (exist ? true : false)
   //    2.2: Parameters: a name followed by a non-option value

#if !defined(__GNUC__) || (__GNUC__ >= 5)
   using wstring_stream = std::wistringstream;
#else
    // Until GCC 5, wistringstream did not have a move constructor.
    // wstringstream_proxy is used instead, as a workaround.
   class wstringstream_proxy
   {
   public:
      wstringstream_proxy() = default;

      // Construct with a value.
      wstringstream_proxy(std::wstring const& value) :
         wstream_(value)
      {}

      // Copy constructor.
      wstringstream_proxy(const wstringstream_proxy& other) :
         wstream_(other.wstream_.str())
      {
         wstream_.setstate(other.wstream_.rdstate());
      }

      void setstate(std::ios_base::iostate state) { wstream_.setstate(state); }

      // Stream out the value of the parameter.
      // If the conversion was not possible, the stream will enter the fail state,
      // and operator bool will return false.
      template<typename T>
      wstringstream_proxy& operator >> (T& thing)
      {
         wstream_ >> thing;
         return *this;
      }


      // Get the string value.
      std::wstring str() const { return wstream_.str(); }

      std::wstringbuf* rdbuf() const { return wstream_.rdbuf(); }

      // Check the state of the stream.
      // False when the most recent stream operation failed
      explicit operator bool() const { return !!wstream_; }

      ~wstringstream_proxy() = default;
   private:
      std::wistringstream wstream_;
   };
   using wstring_stream = wstringstream_proxy;
#endif

   class multimap_iteration_wrapper
   {
   public:
      using container_t = std::multimap<std::wstring, std::wstring>;
      using iterator_t = container_t::const_iterator;
      using difference_t = container_t::difference_type;
      explicit multimap_iteration_wrapper(const iterator_t& lb, const iterator_t& ub)
         : lb_(lb)
         , ub_(ub)
      {}

      iterator_t begin() const { return lb_; }
      iterator_t end() const { return ub_; }
      difference_t size() const { return std::distance(lb_, ub_); }

   private:
      iterator_t lb_;
      iterator_t ub_;
   };

   class wparser
   {
   public:
      enum Mode { PREFER_FLAG_FOR_UNREG_OPTION = 1 << 0,
                  PREFER_PARAM_FOR_UNREG_OPTION = 1 << 1,
                  NO_SPLIT_ON_EQUALSIGN = 1 << 2,
                  SINGLE_DASH_IS_MULTIFLAG = 1 << 3,
                };

      wparser() = default;

      wparser(std::initializer_list<wchar_t const* const> pre_reg_names)
      {  add_params(pre_reg_names); }

      wparser(const wchar_t* const argv[], int mode = PREFER_FLAG_FOR_UNREG_OPTION)
      {  parse(argv, mode); }

      wparser(int argc, const wchar_t* const argv[], int mode = PREFER_FLAG_FOR_UNREG_OPTION)
      {  parse(argc, argv, mode); }

      void add_param(std::wstring const& name);
      void add_params(std::wstring const& name);

      void add_param(std::initializer_list<wchar_t const* const> init_list);
      void add_params(std::initializer_list<wchar_t const* const> init_list);

      void parse(const wchar_t* const argv[], int mode = PREFER_FLAG_FOR_UNREG_OPTION);
      void parse(int argc, const wchar_t* const argv[], int mode = PREFER_FLAG_FOR_UNREG_OPTION);

      std::multiset<std::wstring>               const& flags()    const { return flags_;    }
      std::multimap<std::wstring, std::wstring>  const& params()   const { return params_;   }
      multimap_iteration_wrapper                      params(std::wstring const& name) const;
      std::vector<std::wstring>                 const& pos_args() const { return pos_args_; }

      // begin() and end() for using range-for over positional args.
      std::vector<std::wstring>::const_iterator begin() const { return pos_args_.cbegin(); }
      std::vector<std::wstring>::const_iterator end()   const { return pos_args_.cend();   }
      size_t size()                                    const { return pos_args_.size();   }

      //////////////////////////////////////////////////////////////////////////
      // Accessors

      // flag (boolean) accessors: return true if the flag appeared, otherwise false.
      bool operator[](std::wstring const& name) const;

      // multiple flag (boolean) accessors: return true if at least one of the flag appeared, otherwise false.
      bool operator[](std::initializer_list<wchar_t const* const> init_list) const;

      // returns positional arg string by order. Like argv[] but without the options
      std::wstring const& operator[](size_t ind) const;

      // returns a std::istream that can be used to convert a positional arg to a typed value.
      wstring_stream operator()(size_t ind) const;

      // same as above, but with a default value in case the arg is missing (index out of range).
      template<typename T>
      wstring_stream operator()(size_t ind, T&& def_val) const;

      // parameter accessors, give a name get an std::istream that can be used to convert to a typed value.
      // call .str() on result to get as string
      wstring_stream operator()(std::wstring const& name) const;

      // accessor for a parameter with multiple names, give a list of names, get an std::istream that can be used to convert to a typed value.
      // call .str() on result to get as string
      // returns the first value in the list to be found.
      wstring_stream operator()(std::initializer_list<wchar_t const* const> init_list) const;

      // same as above, but with a default value in case the param was missing.
      // Non-string def_val types must have an operator<<() (output stream operator)
      // If T only has an input stream operator, pass the string version of the type as in "3" instead of 3.
      template<typename T>
      wstring_stream operator()(std::wstring const& name, T&& def_val) const;

      // same as above but for a list of names. returns the first value to be found.
      template<typename T>
      wstring_stream operator()(std::initializer_list<wchar_t const* const> init_list, T&& def_val) const;

   private:
      wstring_stream bad_stream() const;
      std::wstring trim_leading_dashes(std::wstring const& name) const;
      bool is_number(std::wstring const& arg) const;
      bool is_option(std::wstring const& arg) const;
      bool got_flag(std::wstring const& name) const;
      bool is_param(std::wstring const& name) const;

   private:
      std::vector<std::wstring> args_;
      std::multimap<std::wstring, std::wstring> params_;
      std::vector<std::wstring> pos_args_;
      std::multiset<std::wstring> flags_;
      std::set<std::wstring> registeredParams_;
      std::wstring empty_;
   };


   //////////////////////////////////////////////////////////////////////////

   inline void wparser::parse(const wchar_t * const argv[], int mode)
   {
      int argc = 0;
      for (auto argvp = argv; *argvp; ++argc, ++argvp);
      parse(argc, argv, mode);
   }

   //////////////////////////////////////////////////////////////////////////

   inline void wparser::parse(int argc, const wchar_t* const argv[], int mode /*= PREFER_FLAG_FOR_UNREG_OPTION*/)
   {
      // clear out possible previous parsing remnants
      flags_.clear();
      params_.clear();
      pos_args_.clear();

      // convert to strings
      args_.resize(static_cast<decltype(args_)::size_type>(argc));
      std::transform(argv, argv + argc, args_.begin(), [](const wchar_t* const arg) { return arg;  });

      // parse line
      for (auto i = 0u; i < args_.size(); ++i)
      {
         if (!is_option(args_[i]))
         {
            pos_args_.emplace_back(args_[i]);
            continue;
         }

         auto name = trim_leading_dashes(args_[i]);

         if (!(mode & NO_SPLIT_ON_EQUALSIGN))
         {
            auto equalPos = name.find('=');
            if (equalPos != std::wstring::npos)
            {
               params_.insert({ name.substr(0, equalPos), name.substr(equalPos + 1) });
               continue;
            }
         }

         // if the option is unregistered and should be a multi-flag
         if (1 == (args_[i].size() - name.size()) &&          // single dash
            argh::wparser::SINGLE_DASH_IS_MULTIFLAG & mode && // multi-flag mode
            !is_param(name))                                  // unregistered
         {
            std::wstring keep_param;

            if (!name.empty() && is_param(std::wstring(1ul, name.back()))) // last wchar_t is param
            {
               keep_param += name.back();
               name.resize(name.size() - 1);
            }

            for (auto const& c : name)
            {
               flags_.emplace(std::wstring{ c });
            }

            if (!keep_param.empty())
            {
               name = keep_param;
            }
            else
            {
               continue; // do not consider other options for this arg
            }
         }

         // any potential option will get as its value the next arg, unless that arg is an option too
         // in that case it will be determined a flag.
         if (i == args_.size() - 1 || is_option(args_[i + 1]))
         {
            flags_.emplace(name);
            continue;
         }

         // if 'name' is a pre-registered option, then the next arg cannot be a free parameter to it is skipped
         // otherwise we have 2 modes:
         // PREFER_FLAG_FOR_UNREG_OPTION: a non-registered 'name' is determined a flag.
         //                               The following value (the next arg) will be a free parameter.
         //
         // PREFER_PARAM_FOR_UNREG_OPTION: a non-registered 'name' is determined a parameter, the next arg
         //                                will be the value of that option.

         assert(!(mode & argh::wparser::PREFER_FLAG_FOR_UNREG_OPTION)
             || !(mode & argh::wparser::PREFER_PARAM_FOR_UNREG_OPTION));

         bool preferParam = mode & argh::wparser::PREFER_PARAM_FOR_UNREG_OPTION;

         if (is_param(name) || preferParam)
         {
            params_.insert({ name, args_[i + 1] });
            ++i; // skip next value, it is not a free parameter
            continue;
         }
         else
         {
            flags_.emplace(name);
         }
      }
   }

   //////////////////////////////////////////////////////////////////////////

   inline wstring_stream wparser::bad_stream() const
   {
      wstring_stream bad;
      bad.setstate(std::ios_base::failbit);
      return bad;
   }

   //////////////////////////////////////////////////////////////////////////

   inline bool wparser::is_number(std::wstring const& arg) const
   {
      // inefficient but simple way to determine if a string is a number (which can start with a '-')
      std::wistringstream istr(arg);
      double number;
      istr >> number;
      return !(istr.fail() || istr.bad());
   }

   //////////////////////////////////////////////////////////////////////////

   inline bool wparser::is_option(std::wstring const& arg) const
   {
      assert(0 != arg.size());
      if (is_number(arg))
         return false;
      return '-' == arg[0];
   }

   //////////////////////////////////////////////////////////////////////////

   inline std::wstring wparser::trim_leading_dashes(std::wstring const& name) const
   {
      auto pos = name.find_first_not_of('-');
      return std::wstring::npos != pos ? name.substr(pos) : name;
   }

   //////////////////////////////////////////////////////////////////////////

   inline bool argh::wparser::got_flag(std::wstring const& name) const
   {
      return flags_.end() != flags_.find(trim_leading_dashes(name));
   }

   //////////////////////////////////////////////////////////////////////////

   inline bool argh::wparser::is_param(std::wstring const& name) const
   {
      return registeredParams_.count(name);
   }

   //////////////////////////////////////////////////////////////////////////

   inline bool wparser::operator[](std::wstring const& name) const
   {
      return got_flag(name);
   }

   //////////////////////////////////////////////////////////////////////////

   inline bool wparser::operator[](std::initializer_list<wchar_t const* const> init_list) const
   {
      return std::any_of(init_list.begin(), init_list.end(), [&](wchar_t const* const name) { return got_flag(name); });
   }

   //////////////////////////////////////////////////////////////////////////

   inline std::wstring const& wparser::operator[](size_t ind) const
   {
      if (ind < pos_args_.size())
         return pos_args_[ind];
      return empty_;
   }

   //////////////////////////////////////////////////////////////////////////

   inline wstring_stream wparser::operator()(std::wstring const& name) const
   {
      auto optIt = params_.find(trim_leading_dashes(name));
      if (params_.end() != optIt)
         return wstring_stream(optIt->second);
      return bad_stream();
   }

   //////////////////////////////////////////////////////////////////////////

   inline wstring_stream wparser::operator()(std::initializer_list<wchar_t const* const> init_list) const
   {
      for (auto& name : init_list)
      {
         auto optIt = params_.find(trim_leading_dashes(name));
         if (params_.end() != optIt)
            return wstring_stream(optIt->second);
      }
      return bad_stream();
   }

   //////////////////////////////////////////////////////////////////////////

   template<typename T>
   wstring_stream wparser::operator()(std::wstring const& name, T&& def_val) const
   {
      auto optIt = params_.find(trim_leading_dashes(name));
      if (params_.end() != optIt)
         return wstring_stream(optIt->second);

      std::ostringstream ostr;
      ostr.precision(std::numeric_limits<long double>::max_digits10);
      ostr << def_val;
      return wstring_stream(ostr.str()); // use default
   }

   //////////////////////////////////////////////////////////////////////////

   // same as above but for a list of names. returns the first value to be found.
   template<typename T>
   wstring_stream wparser::operator()(std::initializer_list<wchar_t const* const> init_list, T&& def_val) const
   {
      for (auto& name : init_list)
      {
         auto optIt = params_.find(trim_leading_dashes(name));
         if (params_.end() != optIt)
            return wstring_stream(optIt->second);
      }
      std::ostringstream ostr;
      ostr.precision(std::numeric_limits<long double>::max_digits10);
      ostr << def_val;
      return wstring_stream(ostr.str()); // use default
   }

   //////////////////////////////////////////////////////////////////////////

   inline wstring_stream wparser::operator()(size_t ind) const
   {
      if (pos_args_.size() <= ind)
         return bad_stream();

      return wstring_stream(pos_args_[ind]);
   }

   //////////////////////////////////////////////////////////////////////////

   template<typename T>
   wstring_stream wparser::operator()(size_t ind, T&& def_val) const
   {
      if (pos_args_.size() <= ind)
      {
         std::ostringstream ostr;
         ostr.precision(std::numeric_limits<long double>::max_digits10);
         ostr << def_val;
         return wstring_stream(ostr.str());
      }

      return wstring_stream(pos_args_[ind]);
   }

   //////////////////////////////////////////////////////////////////////////

   inline void wparser::add_param(std::wstring const& name)
   {
      registeredParams_.insert(trim_leading_dashes(name));
   }

   //////////////////////////////////////////////////////////////////////////

   inline void wparser::add_param(std::initializer_list<const wchar_t *const> init_list)
   {
       wparser::add_params(init_list);
   }

   //////////////////////////////////////////////////////////////////////////

   inline void wparser::add_params(std::initializer_list<wchar_t const* const> init_list)
   {
      for (auto& name : init_list)
         registeredParams_.insert(trim_leading_dashes(name));
   }

   //////////////////////////////////////////////////////////////////////////

   inline void wparser::add_params(const std::wstring &name)
   {
       wparser::add_param(name);
   }

   //////////////////////////////////////////////////////////////////////////

   inline multimap_iteration_wrapper wparser::params(std::wstring const& name) const
   {
      auto trimmed_name = trim_leading_dashes(name);
      return multimap_iteration_wrapper(params_.lower_bound(trimmed_name), params_.upper_bound(trimmed_name));
   }
}
