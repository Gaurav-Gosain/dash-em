/**
 * @file dashem_addon.c
 * @brief Node.js N-API addon for dash-em
 */

#include <node_api.h>
#include <string.h>
#include "dashem.h"

/**
 * Remove em-dashes from a string
 *
 * Args: [string]
 * Returns: string
 */
static napi_value remove(napi_env env, napi_callback_info info) {
    napi_value result = NULL;
    napi_value argv[1];
    size_t argc = 1;

    napi_status status = napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
    if (status != napi_ok) {
        napi_throw_error(env, "EINVAL", "Failed to get callback info");
        return NULL;
    }

    if (argc < 1) {
        napi_throw_error(env, "EINVAL", "Expected at least 1 argument");
        return NULL;
    }

    char input_buffer[1024 * 1024]; /* 1MB buffer */
    size_t input_len = sizeof(input_buffer) - 1;
    char output_buffer[1024 * 1024];
    size_t output_len = 0;

    /* Get string from argument */
    status = napi_get_value_string_utf8(env, argv[0], input_buffer, input_len, &input_len);
    if (status != napi_ok) {
        napi_throw_error(env, "EINVAL", "Failed to get string value");
        return NULL;
    }

    /* Call C function */
    int ret = dashem_remove(
        input_buffer,
        input_len,
        output_buffer,
        sizeof(output_buffer),
        &output_len
    );

    if (ret != 0) {
        napi_throw_error(env, "ERANGE", "Output buffer too small");
        return NULL;
    }

    /* Create result string */
    status = napi_create_string_utf8(env, output_buffer, output_len, &result);
    if (status != napi_ok) {
        napi_throw_error(env, "EINVAL", "Failed to create string");
        return NULL;
    }

    return result;
}

/**
 * Remove em-dashes from a Buffer (zero-copy, high-performance)
 *
 * Args: [Buffer]
 * Returns: Buffer
 */
static napi_value removeBuffer(napi_env env, napi_callback_info info) {
    napi_value result = NULL;
    napi_value argv[1];
    size_t argc = 1;

    napi_status status = napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
    if (status != napi_ok) {
        napi_throw_error(env, "EINVAL", "Failed to get callback info");
        return NULL;
    }

    if (argc < 1) {
        napi_throw_error(env, "EINVAL", "Expected at least 1 argument");
        return NULL;
    }

    /* Verify argument is a Buffer */
    bool is_buffer = false;
    status = napi_is_buffer(env, argv[0], &is_buffer);
    if (status != napi_ok || !is_buffer) {
        napi_throw_type_error(env, "EINVAL", "Expected Buffer argument");
        return NULL;
    }

    /* Get direct pointer to Buffer data (zero-copy!) */
    char* input_data = NULL;
    size_t input_len = 0;
    status = napi_get_buffer_info(env, argv[0], (void**)&input_data, &input_len);
    if (status != napi_ok) {
        napi_throw_error(env, "EINVAL", "Failed to get buffer info");
        return NULL;
    }

    /* Allocate output buffer dynamically */
    char* output_data = NULL;
    status = napi_create_buffer(env, input_len, (void**)&output_data, &result);
    if (status != napi_ok) {
        napi_throw_error(env, "ENOMEM", "Failed to allocate output buffer");
        return NULL;
    }

    /* Call C function directly on Buffer memory */
    size_t output_len = 0;
    int ret = dashem_remove(
        input_data,
        input_len,
        output_data,
        input_len,
        &output_len
    );

    if (ret != 0) {
        napi_throw_error(env, "ERANGE", "dashem_remove failed");
        return NULL;
    }

    /* Return Buffer slice if output is smaller than input */
    if (output_len < input_len) {
        napi_value slice_method;
        status = napi_get_named_property(env, result, "slice", &slice_method);
        if (status == napi_ok) {
            napi_value args[2];
            napi_create_uint32(env, 0, &args[0]);
            napi_create_uint32(env, (uint32_t)output_len, &args[1]);

            napi_value sliced;
            status = napi_call_function(env, result, slice_method, 2, args, &sliced);
            if (status == napi_ok) {
                result = sliced;
            }
        }
    }

    return result;
}

/**
 * Remove em-dashes from a Buffer in-place (ultra-fast, modifies input!)
 *
 * Args: [Buffer]
 * Returns: number (new length)
 */
static napi_value removeBufferInPlace(napi_env env, napi_callback_info info) {
    napi_value result = NULL;
    napi_value argv[1];
    size_t argc = 1;

    napi_status status = napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
    if (status != napi_ok) {
        napi_throw_error(env, "EINVAL", "Failed to get callback info");
        return NULL;
    }

    if (argc < 1) {
        napi_throw_error(env, "EINVAL", "Expected at least 1 argument");
        return NULL;
    }

    /* Verify argument is a Buffer */
    bool is_buffer = false;
    status = napi_is_buffer(env, argv[0], &is_buffer);
    if (status != napi_ok || !is_buffer) {
        napi_throw_type_error(env, "EINVAL", "Expected Buffer argument");
        return NULL;
    }

    /* Get direct pointer to Buffer data */
    char* data = NULL;
    size_t len = 0;
    status = napi_get_buffer_info(env, argv[0], (void**)&data, &len);
    if (status != napi_ok) {
        napi_throw_error(env, "EINVAL", "Failed to get buffer info");
        return NULL;
    }

    /* Process in-place (input == output) */
    size_t output_len = 0;
    int ret = dashem_remove(data, len, data, len, &output_len);

    if (ret != 0) {
        napi_throw_error(env, "ERANGE", "dashem_remove failed");
        return NULL;
    }

    /* Return new length */
    status = napi_create_uint32(env, (uint32_t)output_len, &result);
    if (status != napi_ok) {
        napi_throw_error(env, "EINVAL", "Failed to create result");
        return NULL;
    }

    return result;
}

/**
 * Get library version
 *
 * Returns: string
 */
static napi_value version(napi_env env, napi_callback_info info) {
    napi_value result;
    const char *version_str = dashem_version();

    napi_status status = napi_create_string_utf8(env, version_str, strlen(version_str), &result);
    if (status != napi_ok) {
        napi_throw_error(env, "EINVAL", "Failed to create version string");
        return NULL;
    }

    return result;
}

/**
 * Get implementation name
 *
 * Returns: string
 */
static napi_value implementationName(napi_env env, napi_callback_info info) {
    napi_value result;
    const char *impl_name = dashem_implementation_name();

    napi_status status = napi_create_string_utf8(env, impl_name, strlen(impl_name), &result);
    if (status != napi_ok) {
        napi_throw_error(env, "EINVAL", "Failed to create implementation name string");
        return NULL;
    }

    return result;
}

/**
 * Module initialization
 */
napi_value init(napi_env env, napi_value exports) {
    napi_status status = napi_ok;

    /* Export functions */
    napi_value remove_fn, version_fn, impl_fn;
    napi_value remove_buffer_fn, remove_buffer_inplace_fn;

    status = napi_create_function(env, "remove", 6, remove, NULL, &remove_fn);
    if (status != napi_ok) {
        napi_throw_error(env, "EINVAL", "Failed to create remove function");
        return NULL;
    }

    status = napi_create_function(env, "version", 7, version, NULL, &version_fn);
    if (status != napi_ok) {
        napi_throw_error(env, "EINVAL", "Failed to create version function");
        return NULL;
    }

    status = napi_create_function(env, "implementationName", 18, implementationName, NULL, &impl_fn);
    if (status != napi_ok) {
        napi_throw_error(env, "EINVAL", "Failed to create implementationName function");
        return NULL;
    }

    status = napi_create_function(env, "removeBuffer", 12, removeBuffer, NULL, &remove_buffer_fn);
    if (status != napi_ok) {
        napi_throw_error(env, "EINVAL", "Failed to create removeBuffer function");
        return NULL;
    }

    status = napi_create_function(env, "removeBufferInPlace", 19, removeBufferInPlace, NULL, &remove_buffer_inplace_fn);
    if (status != napi_ok) {
        napi_throw_error(env, "EINVAL", "Failed to create removeBufferInPlace function");
        return NULL;
    }

    status = napi_set_named_property(env, exports, "remove", remove_fn);
    if (status != napi_ok) {
        napi_throw_error(env, "EINVAL", "Failed to set remove property");
        return NULL;
    }

    status = napi_set_named_property(env, exports, "version", version_fn);
    if (status != napi_ok) {
        napi_throw_error(env, "EINVAL", "Failed to set version property");
        return NULL;
    }

    status = napi_set_named_property(env, exports, "implementationName", impl_fn);
    if (status != napi_ok) {
        napi_throw_error(env, "EINVAL", "Failed to set implementationName property");
        return NULL;
    }

    status = napi_set_named_property(env, exports, "removeBuffer", remove_buffer_fn);
    if (status != napi_ok) {
        napi_throw_error(env, "EINVAL", "Failed to set removeBuffer property");
        return NULL;
    }

    status = napi_set_named_property(env, exports, "removeBufferInPlace", remove_buffer_inplace_fn);
    if (status != napi_ok) {
        napi_throw_error(env, "EINVAL", "Failed to set removeBufferInPlace property");
        return NULL;
    }

    return exports;
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, init)
