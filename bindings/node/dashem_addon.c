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
static napi_value init(napi_env env, napi_exports exports) {
    napi_status status = napi_ok;

    /* Export functions */
    napi_value remove_fn, version_fn, impl_fn;

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

    return exports;
}

NAPI_MODULE(dashem, init)
