cmake_minimum_required(VERSION 3.13)

if(ESP_PLATFORM)
  file(GLOB_RECURSE BME69xLibrary
    LIST_DIRECTORIES false
    "src/*.cpp"
    "src/bme69x/*.c" 
  )

  list(FILTER BME69xLibrary EXCLUDE REGEX ".*/examples/.*")

  idf_component_register(
    SRCS ${BME69xLibrary}
    INCLUDE_DIRS "src" "src/bme69x"
    REQUIRES BME6xxSensor arduino-esp32
  )

  return()
endif()

