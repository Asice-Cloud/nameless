-- event_demo.lua
-- Lua creates coroutines and registers them with C via `c_register(co)`.

-- simple timer task
local function timer_task(name, n, delay)
  print(name .. " start")
  for i=1,n do
    print(string.format("%s tick %d at %s", name, i, os.date("%X")))
    c_sleep(delay)
  end
  print(name .. " done")
end

-- fd-waiting task: waits for a message from a pipe
local function fd_task(name, fd)
  print(name .. " waiting for fd " .. fd)
  local data = c_waitfd(fd) -- C will read and pass data back
  print(name .. " got: '" .. (data or "") .. "'")
end

-- helper: create coroutine and register with C
local function spawn(fn, ...)
  local co = coroutine.create(function(...) fn(...) end)
  c_register(co)
  return co
end

-- spawn tasks
spawn(timer_task, "TimerA", 3, 1)
spawn(timer_task, "TimerB", 2, 2)
spawn(fd_task, "FDReader", PIPE_READ_FD)
