#!/bin/bash

# HIK Unified Bootloader Test Script
# Tests both BIOS and UEFI boot modes

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Print functions
print_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

print_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

print_step() {
    echo -e "${BLUE}[STEP]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[PASS]${NC} $1"
}

print_fail() {
    echo -e "${RED}[FAIL]${NC} $1"
}

# Check dependencies
check_dependencies() {
    print_step "Checking test dependencies..."
    
    local deps=("qemu-system-x86_64" "ovmf" "grep" "timeout")
    local missing=()
    
    for dep in "${deps[@]}"; do
        if ! command -v "$dep" &> /dev/null; then
            missing+=("$dep")
        fi
    done
    
    if [ ${#missing[@]} -ne 0 ]; then
        print_error "Missing dependencies: ${missing[*]}"
        print_info "Install with: sudo apt-get install qemu-system-x86 ovmf"
        exit 1
    fi
    
    print_info "All test dependencies found"
}

# Build bootloaders
build_bootloaders() {
    print_step "Building bootloaders..."
    
    # Build BIOS bootloader
    print_info "Building BIOS bootloader..."
    cd ../boot/tools
    make clean
    make
    cd ../../unified
    
    # Build UEFI bootloader
    print_info "Building UEFI bootloader..."
    cd ../uefi/tools
    make clean
    make
    cd ../../unified
    
    # Build unified image
    print_info "Building unified image..."
    make clean
    make
    
    print_success "Bootloaders built successfully"
}

# Test BIOS boot
test_bios_boot() {
    print_step "Testing BIOS boot..."
    
    local test_image="build/test-bios.img"
    local log_file="build/test-bios.log"
    
    # Create test image
    if [ ! -f "build/hik-unified.img" ]; then
        print_error "Unified image not found. Run 'make' first."
        return 1
    fi
    
    cp build/hik-unified.img "$test_image"
    
    # Run QEMU with BIOS
    print_info "Starting QEMU with BIOS..."
    timeout 30s qemu-system-x86_64 \
        -drive file="$test_image",format=raw \
        -serial stdio \
        -m 512M \
        -nographic \
        2>&1 | tee "$log_file" || true
    
    # Check for success indicators
    if grep -q "HIK Bootloader" "$log_file"; then
        print_success "BIOS boot test passed"
        return 0
    else
        print_fail "BIOS boot test failed"
        return 1
    fi
}

# Test UEFI boot
test_uefi_boot() {
    print_step "Testing UEFI boot..."
    
    local test_image="build/test-uefi.img"
    local log_file="build/test-uefi.log"
    local ovmf_code="/usr/share/OVMF/OVMF_CODE.fd"
    local ovmf_vars="/usr/share/OVMF/OVMF_VARS.fd"
    
    # Create test image
    if [ ! -f "build/hik-unified.img" ]; then
        print_error "Unified image not found. Run 'make' first."
        return 1
    fi
    
    cp build/hik-unified.img "$test_image"
    
    # Check for OVMF
    if [ ! -f "$ovmf_code" ]; then
        print_warn "OVMF not found, trying alternative paths..."
        ovmf_code="/usr/share/edk2-ovmf/x64/OVMF_CODE.fd"
        ovmf_vars="/usr/share/edk2-ovmf/x64/OVMF_VARS.fd"
    fi
    
    if [ ! -f "$ovmf_code" ]; then
        print_error "OVMF not found. Install with: sudo apt-get install ovmf"
        return 1
    fi
    
    # Run QEMU with UEFI
    print_info "Starting QEMU with UEFI..."
    timeout 30s qemu-system-x86_64 \
        -drive file="$test_image",format=raw \
        -drive if=pflash,format=raw,readonly=on,file="$ovmf_code" \
        -drive if=pflash,format=raw,file="$ovmf_vars" \
        -serial stdio \
        -m 512M \
        -nographic \
        2>&1 | tee "$log_file" || true
    
    # Check for success indicators
    if grep -q "HIK Bootloader" "$log_file" || grep -q "EFI" "$log_file"; then
        print_success "UEFI boot test passed"
        return 0
    else
        print_fail "UEFI boot test failed"
        return 1
    fi
}

# Test kernel loading
test_kernel_loading() {
    print_step "Testing kernel loading..."
    
    local log_file="build/kernel-test.log"
    
    # Check kernel image
    if [ ! -f "build/kernel.hik" ]; then
        print_error "Kernel image not found"
        return 1
    fi
    
    # Check kernel header
    local magic=$(xxd -l 8 -p build/kernel.hik)
    if [ "$magic" != "48494b00" ]; then
        print_fail "Invalid kernel magic: $magic"
        return 1
    fi
    
    print_success "Kernel header valid"
    
    # Check kernel size
    local size=$(stat -c%s build/kernel.hik)
    print_info "Kernel size: $size bytes"
    
    if [ $size -lt 1024 ]; then
        print_error "Kernel too small"
        return 1
    fi
    
    print_success "Kernel size valid"
    
    return 0
}

# Test configuration files
test_configurations() {
    print_step "Testing configuration files..."
    
    local passed=0
    local failed=0
    
    # Test BIOS configuration
    if [ -f "../common/boot.cfg" ]; then
        print_info "BIOS configuration exists"
        if grep -q "title" ../common/boot.cfg; then
            print_success "BIOS configuration valid"
            ((passed++))
        else
            print_fail "BIOS configuration invalid"
            ((failed++))
        fi
    else
        print_warn "BIOS configuration not found"
        ((failed++))
    fi
    
    # Test UEFI configuration
    if [ -f "../common/boot.conf" ]; then
        print_info "UEFI configuration exists"
        if grep -q "title" ../common/boot.conf; then
            print_success "UEFI configuration valid"
            ((passed++))
        else
            print_fail "UEFI configuration invalid"
            ((failed++))
        fi
    else
        print_warn "UEFI configuration not found"
        ((failed++))
    fi
    
    print_info "Configuration tests: $passed passed, $failed failed"
    
    return $failed
}

# Run all tests
run_all_tests() {
    print_step "Running all tests..."
    
    local total_tests=4
    local passed_tests=0
    local failed_tests=0
    
    echo ""
    echo "========================================"
    echo "  HIK Unified Bootloader Test Suite"
    echo "========================================"
    echo ""
    
    # Test 1: Build bootloaders
    if build_bootloaders; then
        ((passed_tests++))
    else
        ((failed_tests++))
    fi
    echo ""
    
    # Test 2: Test kernel loading
    if test_kernel_loading; then
        ((passed_tests++))
    else
        ((failed_tests++))
    fi
    echo ""
    
    # Test 3: Test configurations
    if test_configurations; then
        ((passed_tests++))
    else
        ((failed_tests++))
    fi
    echo ""
    
    # Test 4: Test BIOS boot
    if test_bios_boot; then
        ((passed_tests++))
    else
        ((failed_tests++))
    fi
    echo ""
    
    # Test 5: Test UEFI boot
    if test_uefi_boot; then
        ((passed_tests++))
    else
        ((failed_tests++))
    fi
    echo ""
    
    # Print summary
    echo "========================================"
    echo "  Test Summary"
    echo "========================================"
    echo ""
    echo "Total tests: $total_tests"
    echo -e "${GREEN}Passed: $passed_tests${NC}"
    echo -e "${RED}Failed: $failed_tests${NC}"
    echo ""
    
    if [ $failed_tests -eq 0 ]; then
        print_success "All tests passed!"
        return 0
    else
        print_error "Some tests failed"
        return 1
    fi
}

# Main function
main() {
    case "${1:-all}" in
        all)
            check_dependencies
            run_all_tests
            ;;
        bios)
            check_dependencies
            build_bootloaders
            test_bios_boot
            ;;
        uefi)
            check_dependencies
            build_bootloaders
            test_uefi_boot
            ;;
        kernel)
            test_kernel_loading
            ;;
        config)
            test_configurations
            ;;
        clean)
            print_info "Cleaning test artifacts..."
            rm -rf build/test-*.img build/test-*.log build/kernel-test.log
            print_info "Clean complete"
            ;;
        help|--help|-h)
            echo "HIK Unified Bootloader Test Script"
            echo ""
            echo "Usage: $0 [command]"
            echo ""
            echo "Commands:"
            echo "  all     - Run all tests (default)"
            echo "  bios    - Test BIOS boot only"
            echo "  uefi    - Test UEFI boot only"
            echo "  kernel  - Test kernel loading only"
            echo "  config  - Test configuration files only"
            echo "  clean   - Clean test artifacts"
            echo "  help    - Show this help message"
            ;;
        *)
            print_error "Unknown command: $1"
            echo "Run '$0 help' for usage information"
            exit 1
            ;;
    esac
}

# Run main function
main "$@"