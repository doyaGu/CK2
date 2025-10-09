# CK2 - Virtools Behavioral Engine

CK2 is a modernized implementation of the Virtools Behavioral Engine, a comprehensive C++ framework for interactive 3D applications, multimedia content creation, and behavioral scripting. This project represents a clean-room reimplementation of the core Virtools SDK functionality with modern build systems and improved architecture.

## Features

- **Object-Oriented 3D Framework**: Complete hierarchy of 3D entities, meshes, cameras, lights, and materials
- **Behavioral System**: Visual scripting engine with behaviors, parameters, and message passing
- **Manager Architecture**: Modular subsystem management for rendering, input, sound, timing, and more
- **Parameter System**: Type-safe parameter operations with automatic serialization
- **Scene Management**: Hierarchical scene organization with levels, groups, and dependencies
- **File I/O**: Save/load system with state chunk serialization
- **Cross-Platform Math**: VxMath integration for vectors, matrices, and geometric operations

## Architecture

### Core Components

- **CKContext**: Central hub managing the entire engine ecosystem
- **CKObject**: Base class for all engine objects with ID management and serialization
- **CKBeObject**: Behavioral objects that can execute scripts and respond to messages
- **CK3dEntity**: 3D objects with transformation matrices and hierarchical relationships
- **CKBehavior**: Visual scripting behaviors with input/output parameters
- **Managers**: Specialized subsystems (Object, Parameter, Behavior, Message, Time, etc.)

### Key Directories

```
├── include/          # Public API headers
├── src/              # Implementation files
├── deps/             # Third-party dependencies
├── tests/            # Test suite and Player application
├── cmake/            # CMake configuration modules
└── build/            # Build output (generated)
```

## Building

### Prerequisites

- **Windows**: This project currently targets Windows only
- **CMake**: Version 3.12 or higher
- **Visual Studio**: 2017 or later (MSVC compiler)
- **Git**: For dependency management

### Quick Start

```bash
# Clone the repository
git clone <repository-url>
cd CK2

# Configure the build
cmake -B build

# Build the project
cmake --build build

# Build with specific configuration
cmake --build build --config Release
```

### Build Options

- `CK2_BUILD_TESTS=ON/OFF`: Build test suite and Player application (default: OFF)

### With Tests

```bash
# Configure with tests enabled
cmake -B build -DCK2_BUILD_TESTS=ON

# Build everything including tests
cmake --build build

# Run the test suite
ctest --test-dir build

# Run the Player application
./build/tests/Player.exe
```

## Testing

The project includes a comprehensive test suite built with Google Test:

```bash
# Run all tests
ctest --test-dir build --verbose

# Run specific test
./build/tests/PlayerTest.exe

# Run the Player application (interactive)
./build/tests/Player.exe
```

## API Overview

### Basic Usage

```cpp
#include "CKAll.h"

// Create a context (engine instance)
CKContext* context = CKCreateContext();

// Create a 3D object
CK3dEntity* entity = (CK3dEntity*)context->CreateObject(CKCID_3DENTITY, "MyObject");

// Set position
VxVector position(10.0f, 0.0f, 5.0f);
entity->SetPosition(&position);

// Create and attach a behavior
CKBehavior* behavior = (CKBehavior*)context->CreateObject(CKCID_BEHAVIOR, "MyBehavior");
entity->AddBehavior(behavior);

// Clean up
CKCloseContext(context);
```

### Manager Access

```cpp
// Get manager instances
CKObjectManager* objMgr = context->GetObjectManager();
CKParameterManager* paramMgr = context->GetParameterManager(); 
CKBehaviorManager* behaviorMgr = context->GetBehaviorManager();

// Object operations
CKObject* obj = objMgr->GetObject(objectId);
int objCount = objMgr->GetObjectsCountByClassID(CKCID_3DENTITY);
```

## Dependencies

- **VxMath.lib**: Core mathematics library (vectors, matrices, quaternions)
- **miniz**: Compression/decompression for file formats
- **yyjson**: High-performance JSON parsing
- **spdlog**: Fast C++ logging library
- **cpp-dump**: Debug output and object inspection
- **Google Test**: Unit testing framework (test builds only)

## Project Structure

The codebase follows the original Virtools architecture with modern improvements:

- Headers in `include/` define the public API
- Implementations in `src/` provide the core functionality
- Manager classes handle specialized subsystems
- Object hierarchy provides polymorphic 3D entity system
- Parameter system enables visual scripting workflows

## Contributing

1. Follow the existing code style and conventions
2. Ensure all tests pass before submitting changes
3. Add tests for new functionality
4. Update documentation for public API changes

## License

This project is licensed under the Apache License 2.0 - see the [LICENSE](LICENSE) file for details.

## Related Projects

This is part of the larger NeMo2 project, which aims to modernize and preserve the Virtools ecosystem for contemporary development environments.

---

*CK2 brings the powerful Virtools behavioral engine into the modern era while maintaining compatibility with classic Virtools workflows and methodologies.*