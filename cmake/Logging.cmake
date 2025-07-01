function(align_text TEXT TARGET_LENGTH OUTPUT_VARIABLE)
  # Приведение длины строки к целевому числу символов
  string(LENGTH "${TEXT}" TEXT_LENGTH)
  math(EXPR PADDING_LENGTH "${TARGET_LENGTH} - ${TEXT_LENGTH}")

  if(PADDING_LENGTH GREATER 0)
    # Добавление пробелов для выравнивания
    string(REPEAT " " ${PADDING_LENGTH} PADDING)
    set(RESULT "${TEXT}${PADDING}")
  else()
    # Обрезка строки до целевой длины
    string(SUBSTRING "${TEXT}" 0 ${TARGET_LENGTH} RESULT)
  endif()

  # Возврат результата в указанную переменную
  set(${OUTPUT_VARIABLE}
      "${RESULT}"
      PARENT_SCOPE)
endfunction()

function(find_max_length OUTPUT_VARIABLE)
  # Проверяем, что переданы аргументы
  if(NOT ARGV)
    message(FATAL_ERROR "No words provided to find_max_length function")
  endif()

  # Инициализируем максимальную длину
  set(MAX_LEN 0)

  # Проходим по каждому слову
  foreach(WORD IN LISTS ARGV)
    string(LENGTH "${WORD}" WORD_LENGTH)
    if(WORD_LENGTH GREATER MAX_LEN)
      set(MAX_LEN ${WORD_LENGTH})
    endif()
  endforeach()

  # Возвращаем максимальную длину
  set(${OUTPUT_VARIABLE}
      ${MAX_LEN}
      PARENT_SCOPE)
endfunction()

find_max_length(
  MAX_LEN
  "TRACE"
  "DEBUG"
  "INFO"
  "WARNING"
  "ERROR"
  "FATAL")

math(EXPR MAX_LEN "${MAX_LEN} + 2") # Добавим еще два символа (пробел и :)

macro(log_trace MSG)
  align_text("TRACE: " ${MAX_LEN} ALIGNED_TEXT)
  message(STATUS ${ALIGNED_TEXT} ${MSG})
endmacro()

macro(log_debug MSG)
  align_text("DEBUG: " ${MAX_LEN} ALIGNED_TEXT)
  message(STATUS ${ALIGNED_TEXT} ${MSG})
endmacro()

macro(log_info MSG)
  align_text("INFO: " ${MAX_LEN} ALIGNED_TEXT)
  message(STATUS ${ALIGNED_TEXT} ${MSG})
endmacro()

macro(log_warning MSG)
  align_text("WARNING: " ${MAX_LEN} ALIGNED_TEXT)
  message(WARNING ${ALIGNED_TEXT} ${MSG})
endmacro()

macro(log_error MSG)
  align_text("ERROR: " ${MAX_LEN} ALIGNED_TEXT)
  message(SEND_ERROR ${ALIGNED_TEXT} ${MSG}) # Ошибка, но выоплнение продолжится
endmacro()

macro(log_fatal MSG)
  align_text("FATAL: " ${MAX_LEN} ALIGNED_TEXT)
  message(FATAL_ERROR ${ALIGNED_TEXT} ${MSG})
endmacro()
