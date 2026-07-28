print("Calling C function 'add' via dynref:")
local r = dynref.call("add", 11, 31)
print(" add(11,31) =>", r)

print("Calling C function 'greet' via dynref:")
local s = dynref.call("greet", "Alice")
print(" greet('Alice') =>", s)

local function fib(n)
  if n <= 1 then return n end
  return fib(n-1) + fib(n-2)
end
print ("Calculating fib(10) in Lua:")
local result = fib(10)
print(" fib(10) =>", result)