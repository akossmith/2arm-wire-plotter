#pragma once

#include "utils.h"

///////////////////////////////////////////////////////////////////////////
/// Base class for NVM-enabled components
///////////////////////////////////////////////////////////////////////////
template <typename T>
struct WithNVMData {
 public:
  using data_type = T;

 protected:
  mutable data_type nvmData;

 private:
  template <uint16_t index, class... Ts>
  friend class NVMDataAggregate;

  template <template <typename> typename, typename... Ts>
  friend class NVMManager;

 public:
  WithNVMData() {}
  WithNVMData(T val) : nvmData(val) {}

 protected:
  virtual void  afterLoadFromNVM() = 0;  //< called after externally initiated restore
  virtual void beforeSaveToNVM() const = 0;  //< called after externally initiated save

  void setNVMData(const T& val) {
    static_cast<T&>(nvmData) = val;
    afterLoadFromNVM();
  }

  const T& getNVMData() const {
    beforeSaveToNVM();
    return static_cast<const T&>(nvmData);
  }
};

///////////////////////////////////////////////////////////////////////////
/// Recursive aggregate type collecting the NVM data of all NVM-utilizing
///////////////////////////////////////////////////////////////////////////
template <uint16_t index, class... Ts>
class NVMDataAggregate {
 public:
  template <uint16_t N>
  struct typeOfNth {
    // static_assert(false);
  };

  static constexpr uint16_t length() { return 0; }

  template <uint16_t>
  void at() {}
};

template <uint16_t index, class T, class... Ts>
class NVMDataAggregate<index, T, Ts...>
    : public NVMDataAggregate<index + 1, Ts...> {
  using base_class = NVMDataAggregate<index + 1, Ts...>;
  using data_type = typename T::data_type;
  typename T::data_type head;  // cache. Kept up-to-date with nvm

 public:
  NVMDataAggregate(T& t, Ts&... ts)
      : NVMDataAggregate<index + 1, Ts...>(ts...),
        head(static_cast<WithNVMData<typename T::data_type>&>(t).getNVMData()) {
  }

  NVMDataAggregate()
      : NVMDataAggregate<index + 1, Ts...>(), head(typename T::data_type()) {}

  static constexpr uint16_t length() { return base_class::length() + 1; }

  using base_class::at;

  template <uint16_t ind>
  enable_if_t<ind == index, data_type&> at() {
    return head;
  }

  template <uint16_t ind>
  enable_if_t<ind == index, const data_type&> at() const {
    return head;
  }
};

///////////////////////////////////////////////////////////////////////////
/// NVMManager
///////////////////////////////////////////////////////////////////////////

template <template <typename> typename flash_manager_type, typename... Ts>
class NVMManager {
 public:
  using nvm_data_aggregate_type = NVMDataAggregate<0, Ts...>;

 protected:
  nvm_data_aggregate_type nvmDataAggregate;
  flash_manager_type<nvm_data_aggregate_type> flashManager;

 public:
  /// constructor used for priming nvm for very first use
  /// using default ctors of nvmData fragments
  NVMManager(Ts&... ts) : nvmDataAggregate(ts...) {
    flashManager.putFirst(nvmDataAggregate);
  }

  NVMManager() : nvmDataAggregate{} { flashManager.get(nvmDataAggregate); }

  const class EndBurst {
  } endBurst{};

  /// Proxy object and scope guard for burst updates, i.e. modifying several
  /// values in NVM with only a single physical write operation at the end.
  /// Usage: burstUpdater << component1 << component3 [<< endBurst];
  class BurstUpdater {
    NVMDataAggregate<0, Ts...>& nvmDataAggregate;
    flash_manager_type<nvm_data_aggregate_type>& flashManager;
    bool saved = true;

   public:
    BurstUpdater(flash_manager_type<nvm_data_aggregate_type>& flashManager,
                 NVMDataAggregate<0, Ts...>& nvmDataAggregate)
        : nvmDataAggregate(nvmDataAggregate), flashManager(flashManager) {}

    BurstUpdater& operator=(const BurstUpdater& other) {
      if (&other != this) {
        *this = other;
        other.saved = true;
      }
      return *this;
    }

    BurstUpdater& save() {
      flashManager.put(nvmDataAggregate);
      saved = true;
      return *this;
    }

    ~BurstUpdater() {
      if (!saved) {
        save();
      }
    }

    template <uint16_t ind, typename U>
    BurstUpdater& persist(const U& originator) {
      saved = false;
      nvmDataAggregate.template at<ind>() = originator.getNVMData();
      return *this;
    }
  };

  friend class BurstUpdater;

  BurstUpdater persistBurst() {
    return BurstUpdater(flashManager, nvmDataAggregate);
  }

  template <uint16_t ind, typename U>
  void persist(const U& originator) {
    persistBurst().template persist<ind>(originator);
    // nvmDataAggregate.at<index>() = originator.getNVMData();
    // persistBurst() << value << endBurst;
  }

  /// update contents of nvmData of 'originator' to match that of the NVM
  template <uint16_t ind, typename U>
  void restore(U& originator) {
    originator.setNVMData(nvmDataAggregate.template at<ind>());
    // originator.nvmData = nvmDataAggregate.at<index>();
  }
};