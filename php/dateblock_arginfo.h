/* The function's signature, as described in dateblock.stub.php */

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_dateblock, 0, 1, DateTimeImmutable, 0)
	ZEND_ARG_TYPE_INFO(0, cron, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, block, _IS_BOOL, 0, "true")
	ZEND_ARG_OBJ_TYPE_MASK(0, ref, DateTimeInterface, MAY_BE_LONG|MAY_BE_DOUBLE|MAY_BE_NULL, "null")
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, isc, _IS_BOOL, 0, "false")
ZEND_END_ARG_INFO()

ZEND_FUNCTION(dateblock);

static const zend_function_entry ext_functions[] = {
	ZEND_FE(dateblock, arginfo_dateblock)
	ZEND_FE_END
};
