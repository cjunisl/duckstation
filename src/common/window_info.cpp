#include "window_info.h"

#if defined(_WIN32)

#include "common/windows_headers.h"
#include <dwmapi.h>

static bool GetRefreshRateFromDWM(HWND hwnd, float* refresh_rate)
{
  static HMODULE dwm_module = nullptr;
  static HRESULT(STDAPICALLTYPE * is_composition_enabled)(BOOL * pfEnabled) = nullptr;
  static HRESULT(STDAPICALLTYPE * get_timing_info)(HWND hwnd, DWM_TIMING_INFO * pTimingInfo) = nullptr;
  static bool load_tried = false;
  if (!load_tried)
  {
    load_tried = true;
    dwm_module = LoadLibrary("dwmapi.dll");
    if (dwm_module)
    {
      std::atexit([]() {
        FreeLibrary(dwm_module);
        dwm_module = nullptr;
      });
      is_composition_enabled =
        reinterpret_cast<decltype(is_composition_enabled)>(GetProcAddress(dwm_module, "DwmIsCompositionEnabled"));
      get_timing_info =
        reinterpret_cast<decltype(get_timing_info)>(GetProcAddress(dwm_module, "DwmGetCompositionTimingInfo"));
    }
  }

  BOOL composition_enabled;
  if (!is_composition_enabled || FAILED(is_composition_enabled(&composition_enabled) || !get_timing_info))
    return false;

  DWM_TIMING_INFO ti = {};
  ti.cbSize = sizeof(ti);
  HRESULT hr = get_timing_info(nullptr, &ti);
  if (SUCCEEDED(hr))
  {
    if (ti.rateRefresh.uiNumerator == 0 || ti.rateRefresh.uiDenominator == 0)
      return false;

    *refresh_rate = static_cast<float>(ti.rateRefresh.uiNumerator) / static_cast<float>(ti.rateRefresh.uiDenominator);
    return true;
  }

  return false;
}

static bool GetRefreshRateFromMonitor(HWND hwnd, float* refresh_rate)
{
  HMONITOR mon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
  if (!mon)
    return false;

  MONITORINFOEXW mi = {};
  mi.cbSize = sizeof(mi);
  if (GetMonitorInfoW(mon, &mi))
  {
    DEVMODEW dm = {};
    dm.dmSize = sizeof(dm);

    // 0/1 are reserved for "defaults".
    if (EnumDisplaySettingsW(mi.szDevice, ENUM_CURRENT_SETTINGS, &dm) && dm.dmDisplayFrequency > 1)
    {
      *refresh_rate = static_cast<float>(dm.dmDisplayFrequency);
      return true;
    }
  }

  return false;
}

bool WindowInfo::QueryRefreshRateForWindow(const WindowInfo& wi, float* refresh_rate)
{
  if (wi.type != Type::Win32 || !wi.window_handle)
    return false;

  // Try DWM first, then fall back to integer values.
  const HWND hwnd = static_cast<HWND>(wi.window_handle);
  return GetRefreshRateFromDWM(hwnd, refresh_rate) || GetRefreshRateFromMonitor(hwnd, refresh_rate);
}

#else

bool WindowInfo::QueryRefreshRateForWindow(const WindowInfo& wi, float* refresh_rate)
{
  return false;
}

#endif