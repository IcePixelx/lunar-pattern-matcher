# lunar-pattern-matcher

Requires C++14 or later, mainly tested on C++17.

**How to use:**

The file `lunar_pattern_matcher.h` provides multiple utilies that one can use.

Finding an ida-style pattern with macros:
```cpp
LPM_DEF_PAT("48 89 5C ? 20 57 41 54", TestPattern); // LPN_DEF_PAT needs to be defined globally.

// In function
// LPM_FIND_PAT takes the name of the defined pattern, the data pointer and the length of the data.
uint8_t* address = LPM_FIND_PAT(TestPattern, data, dataLen);
```

Finding an ida-style pattern without macros:
```cpp
// Everything here needs to be defined globally.
constexpr const char testPattern[] = "48 89 5C ? 20 57 41 54";
constexpr lpm::CIDAToCodePattern<testPattern> testCodePattern = lpm::CIDAToCodePattern<testPattern>();
constexpr lpm::CPatternContainer testPatternContainer = lpm::CPatternContainer(testCodePattern.GetPattern(), testCodePattern.GetMask());

// In function
uint8_t* address = lpm::FindPattern(testPatternContainer, data, dataLen);
```

Finding a code-style pattern without macros:
```cpp
constexpr lpm::CPatternContainer testPatternContainer = lpm::CPatternContainer("\x48\x89\x5C\xAA\x20\x24\x50", "xxx?xxx");
uint8_t* address = lpm::FindPattern(testPatternContainer, data, dataLen);
```

# Issues
1. For ida-style patterns the conversion will leave the ida-style pattern in .rdata even if it's not referenced throughout the binary anymore.
2. CPatternContainer currently does not support code-patterns that contain \x00, so no null-terminators in your patterns. This is due to an issue with the compile-time constructor **This will be fixed soon**
