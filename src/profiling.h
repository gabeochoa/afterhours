#pragma once

// Profiling system integration with spall
// Define ENABLE_PROFILING to enable profiling, or DEBUG to enable it automatically
// Define RELEASE to disable profiling

#if defined(RELEASE)
    #undef ENABLE_PROFILING
#elif defined(DEBUG) || defined(AFTER_HOURS_DEBUG)
    #ifndef ENABLE_PROFILING
        #define ENABLE_PROFILING
    #endif
#endif

#ifdef ENABLE_PROFILING
    #include "../vendor/spall.h"
    #include <chrono>
    #include <string>
    #include <thread>
    #include <mutex>
    
    namespace profiling {
        
        class Profiler {
        private:
            SpallProfile ctx;
            SpallBuffer buffer;
            std::mutex buffer_mutex;
            bool initialized;
            
            static uint64_t get_timestamp() {
                auto now = std::chrono::high_resolution_clock::now();
                auto duration = now.time_since_epoch();
                return std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
            }
            
        public:
            Profiler() : initialized(false) {
                memset(&ctx, 0, sizeof(ctx));
                memset(&buffer, 0, sizeof(buffer));
            }
            
            ~Profiler() {
                if (initialized) {
                    spall_quit(&ctx);
                }
            }
            
            bool init_file(const std::string& filename) {
                if (initialized) return false;
                
                // Initialize with nanosecond precision
                if (!spall_init_file(filename.c_str(), 1e-9, &ctx)) {
                    return false;
                }
                
                // Initialize buffer (64KB should be enough for most cases)
                const size_t buffer_size = 64 * 1024;
                buffer.data = malloc(buffer_size);
                if (!buffer.data) {
                    spall_quit(&ctx);
                    return false;
                }
                
                buffer.length = buffer_size;
                buffer.tid = static_cast<uint32_t>(std::hash<std::thread::id>{}(std::this_thread::get_id()));
                buffer.pid = static_cast<uint32_t>(getpid());
                
                if (!spall_buffer_init(&ctx, &buffer)) {
                    free(buffer.data);
                    spall_quit(&ctx);
                    return false;
                }
                
                // Name the process
                spall_buffer_name_process(&ctx, &buffer, "AfterHours", 11);
                
                initialized = true;
                return true;
            }
            
            bool begin_event(const std::string& name, const std::string& args = "") {
                if (!initialized) return false;
                
                std::lock_guard<std::mutex> lock(buffer_mutex);
                return spall_buffer_begin_args(&ctx, &buffer, 
                    name.c_str(), static_cast<int32_t>(name.length()),
                    args.c_str(), static_cast<int32_t>(args.length()),
                    get_timestamp());
            }
            
            bool end_event() {
                if (!initialized) return false;
                
                std::lock_guard<std::mutex> lock(buffer_mutex);
                return spall_buffer_end(&ctx, &buffer, get_timestamp());
            }
            
            bool flush() {
                if (!initialized) return false;
                
                std::lock_guard<std::mutex> lock(buffer_mutex);
                return spall_buffer_flush(&ctx, &buffer);
            }
            
            void shutdown() {
                if (!initialized) return;
                
                std::lock_guard<std::mutex> lock(buffer_mutex);
                spall_buffer_quit(&ctx, &buffer);
                if (buffer.data) {
                    free(buffer.data);
                    buffer.data = nullptr;
                }
                spall_quit(&ctx);
                initialized = false;
            }
            
            bool is_initialized() const { return initialized; }
        };
        
        // Global profiler instance
        extern Profiler g_profiler;
        
        // RAII profiler scope
        class ProfileScope {
        private:
            std::string name;
            bool active;
            
        public:
            ProfileScope(const std::string& event_name, const std::string& args = "") 
                : name(event_name), active(false) {
                if (g_profiler.is_initialized()) {
                    active = g_profiler.begin_event(event_name, args);
                }
            }
            
            ~ProfileScope() {
                if (active) {
                    g_profiler.end_event();
                }
            }
        };
        
        // Convenience macros for profiling
        #define PROFILE_SCOPE(name) profiling::ProfileScope profile_scope_##__LINE__(name)
        #define PROFILE_SCOPE_ARGS(name, args) profiling::ProfileScope profile_scope_##__LINE__(name, args)
        #define PROFILE_BEGIN(name) if (profiling::g_profiler.is_initialized()) profiling::g_profiler.begin_event(name)
        #define PROFILE_END() if (profiling::g_profiler.is_initialized()) profiling::g_profiler.end_event()
        #define PROFILE_FLUSH() if (profiling::g_profiler.is_initialized()) profiling::g_profiler.flush()
        
    } // namespace profiling
    
#else
    // Profiling disabled - provide no-op implementations
    namespace profiling {
        class Profiler {
        public:
            Profiler() = default;
            ~Profiler() = default;
            bool init_file(const std::string&) { return false; }
            bool begin_event(const std::string&, const std::string& = "") { return false; }
            bool end_event() { return false; }
            bool flush() { return false; }
            void shutdown() {}
            bool is_initialized() const { return false; }
        };
        
        class ProfileScope {
        public:
            ProfileScope(const std::string&, const std::string& = "") {}
            ~ProfileScope() = default;
        };
        
        extern Profiler g_profiler;
        
        #define PROFILE_SCOPE(name) (void)0
        #define PROFILE_SCOPE_ARGS(name, args) (void)0
        #define PROFILE_BEGIN(name) (void)0
        #define PROFILE_END() (void)0
        #define PROFILE_FLUSH() (void)0
        
    } // namespace profiling
#endif