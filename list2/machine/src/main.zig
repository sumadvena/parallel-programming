const std = @import("std");
const stdout = std.io.getStdOut().writer();
const Thread = std.Thread;

const PARENTS: u8 = 4;
const CHILDREN: u8 = 3;
const DEPTH: usize = 1000;
const JOBS_NUM: u8 = 48;
const FILENAME = "data4.txt";

var jobTable: []Job = undefined;
var arenaTable = std.heap.ArenaAllocator.init(std.heap.page_allocator);
var threadedArenaTable: std.heap.ThreadSafeAllocator = .{ .child_allocator = arenaTable.allocator() };
var arena = threadedArenaTable.allocator();

var stdoutMutex: Thread.Mutex = .{};
var lock: Thread.RwLock = .{};

const Job = struct { r: usize, p: usize, q: usize };

const Instance = struct {
    order: [JOBS_NUM]usize,
    max_completion_time: usize,

    fn calculateCost(self: *Instance) void {
        self.max_completion_time = 0;
        var time: usize = 0;
        for (0..self.order.len) |i| {
            time = @max(jobTable[self.order[i]].r, time);
            time += jobTable[self.order[i]].p;
            self.max_completion_time = @max(self.max_completion_time, time + jobTable[self.order[i]].q);
        }
    }
};

fn allocateTable(size: u8) ![]Job {
    var table: []Job = undefined;
    table = try arena.alloc(Job, size);
    return table;
}

fn readData(filename: []const u8) !void {
    var file = try std.fs.cwd().openFile(filename, .{});
    defer file.close();

    var buffered = std.io.bufferedReader(file.reader());
    var bufferReader = buffered.reader();

    var buffer: [1024]u8 = undefined;
    @memset(buffer[0..], 0);
    var i: u8 = 0;

    while (try bufferReader.readUntilDelimiterOrEof(buffer[0..], '\n')) |chunk| : (i += 1) {
        var rpq: usize = 0;
        var iter = std.mem.splitAny(u8, chunk, " ");
        while (iter.next()) |element| {
            if (i == 0) {
                try stdout.print("Number of jobs: {d}\n", .{JOBS_NUM});
                jobTable = try allocateTable(JOBS_NUM);
            }
            if (i > 0 and i < JOBS_NUM + 1) {
                const value = try std.fmt.parseUnsigned(usize, element, 10);
                switch (rpq) {
                    0 => jobTable[i - 1].r = value,
                    1 => jobTable[i - 1].p = value,
                    2 => jobTable[i - 1].q = value,
                    else => std.debug.print("Wrong r/p/q value\n", .{}),
                }
                rpq += 1;
            }
        }
    }

    try stdout.print("Jobs: \n{any}\n", .{jobTable});
}

fn generateOrder(order: []usize) !void {
    var prng = std.Random.DefaultPrng.init(blk: {
        var seed: u64 = undefined;
        try std.posix.getrandom(std.mem.asBytes(&seed));
        break :blk seed;
    });

    for (0..order.len) |i| {
        order[i] = i;
    }

    std.Random.shuffle(prng.random(), usize, order);
}

fn generateParent(parent: *Instance) !void {
    try generateOrder(&parent.order);
    parent.calculateCost();

    stdoutMutex.lock();
    try stdout.print("{any}\t{}\n", .{ parent.order, parent.max_completion_time });
    stdoutMutex.unlock();
    var i: usize = 0;
    while (i < DEPTH) : (i += 1) {
        try populate(parent);
    }
}

fn evaluateParent(parents: []const Instance) usize {
    var bestCTime: usize = std.math.maxInt(usize);
    var bestInstance: usize = undefined;
    for (0..parents.len) |i| {
        if (parents[i].max_completion_time < bestCTime) {
            bestCTime = parents[i].max_completion_time;
            bestInstance = i;
        }
    }
    return bestInstance;
}

fn pivotSwap(parent: *Instance, pivotPoints: []const usize, childOrder: []usize) void {
    lock.lockShared();
    for (0..childOrder.len) |i| {
        childOrder[i] = parent.order[i];
    }
    lock.unlockShared();

    for (pivotPoints) |pivot| {
        if (pivot == 0) {
            std.mem.swap(usize, &childOrder[pivot], &childOrder[childOrder.len - 1]);
        } else if (pivot == childOrder.len - 1) {
            std.mem.swap(usize, &childOrder[pivot], &childOrder[0]);
        } else {
            std.mem.swap(usize, &childOrder[pivot], &childOrder[pivot - 1]);
        }
    }
}

fn valueInArray(value: usize, array: []const usize) bool {
    for (array) |num| {
        if (value == num) {
            return true;
        }
    }
    return false;
}

fn createChild(child: *Instance, parent: *Instance) !void {
    const rand = std.crypto.random;
    var pivotPoints: [JOBS_NUM / 4]usize = undefined;
    for (&pivotPoints) |*point| {
        var randomPoint: usize = rand.uintLessThan(usize, JOBS_NUM);
        while (valueInArray(randomPoint, &pivotPoints)) {
            randomPoint = rand.uintLessThan(usize, JOBS_NUM);
        }
        point.* = randomPoint;
    }

    pivotSwap(parent, &pivotPoints, &child.order);
    child.calculateCost();

    stdoutMutex.lock();
    try stdout.print("{any} \t{}\n", .{ child.order, child.max_completion_time });
    stdoutMutex.unlock();
}

fn populate(parent: *Instance) !void {
    var children: [CHILDREN]Instance = undefined;
    var threads: [CHILDREN]Thread = undefined;

    for (0..children.len) |i| {
        threads[i] = try Thread.spawn(.{}, createChild, .{ &children[i], parent });
    }

    for (0..threads.len) |i| {
        threads[i].join();
        if (children[i].max_completion_time < parent.max_completion_time) {
            for (0..parent.order.len) |j| {
                parent.order[j] = children[i].order[j];
            }
            parent.calculateCost();
        }
    }
}

pub fn main() !void {
    try readData(FILENAME);
    defer arenaTable.deinit();

    var threads: [PARENTS]Thread = undefined;
    var parentsTable: [PARENTS]Instance = undefined;
    for (0..threads.len) |i| {
        threads[i] = try Thread.spawn(.{}, generateParent, .{&parentsTable[i]});
    }

    for (0..threads.len) |i| {
        threads[i].join();
    }
    const bestInstance: usize = evaluateParent(&parentsTable);
    try stdout.print("\n\nThe best found cost is {} where order is: \n", .{parentsTable[bestInstance].max_completion_time});
    for (parentsTable[bestInstance].order) |elem| {
        try stdout.print("{} ", .{elem});
    }
    try stdout.print("\n", .{});
}

/// This imports the separate module containing `root.zig`. Take a look in `build.zig` for details.
const lib = @import("qap_lib");
