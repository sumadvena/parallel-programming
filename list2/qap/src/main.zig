const std = @import("std");
const stdout = std.io.getStdOut().writer();
const Thread = std.Thread;

const PARENTS: u8 = 4;
const CHILDREN: u8 = 3;
const DEPTH: usize = 100000;
const DIMENSION: u8 = 20;
const FILENAME = "had20.dat";

var distanceMatrix: [][]u8 = undefined;
var flowMatrix: [][]u8 = undefined;
var arenaMatrix = std.heap.ArenaAllocator.init(std.heap.page_allocator);
var threadedArenaMatrix: std.heap.ThreadSafeAllocator = .{ .child_allocator = arenaMatrix.allocator() };
var arena = threadedArenaMatrix.allocator();

var stdoutMutex: Thread.Mutex = .{};
var lock: Thread.RwLock = .{};

const Instance = struct {
    order: [DIMENSION]usize,
    cost: usize,

    fn calculateCost(self: *Instance) void {
        var cost: usize = 0;
        for (0..self.order.len) |i| {
            for (0..self.order.len) |j| {
                cost += flowMatrix[i][j] * distanceMatrix[self.order[i]][self.order[j]];
            }
        }
        self.cost = cost;
    }
};

fn allocateMatrix(size: u8) ![][]u8 {
    var matrix: [][]u8 = undefined;
    matrix = try arena.alloc([]u8, size);
    for (matrix) |*row| {
        row.* = try arena.alloc(u8, size);
    }
    return matrix;
}

fn printMatrix(matrix: [][]u8) !void {
    for (matrix) |col| {
        for (col) |row| {
            try stdout.print("{d}  ", .{row});
        }
        try stdout.print("\n", .{});
    }
    try stdout.print("\n", .{});
}

fn addNextToMatrix(columnPtr: *u8, rowPtr: *u8, distance: bool, element: []const u8) !void {
    const number = try std.fmt.parseUnsigned(u8, element, 10);
    const col = columnPtr.*;
    const row = rowPtr.*;
    switch (distance) {
        true => distanceMatrix[col][row] = number,
        false => flowMatrix[col][row] = number,
    }
    if (rowPtr.* == DIMENSION - 1) {
        if (columnPtr.* == DIMENSION - 1) {
            columnPtr.* = 0;
        } else {
            columnPtr.* += 1;
        }
        rowPtr.* = 0;
    } else {
        rowPtr.* += 1;
    }
}

fn readData(filename: []const u8) !void {
    var file = try std.fs.cwd().openFile(filename, .{});
    defer file.close();

    var buffered = std.io.bufferedReader(file.reader());
    var bufferReader = buffered.reader();

    var buffer: [1024]u8 = undefined;
    @memset(buffer[0..], 0);
    var i: u8 = 0;
    var k: u8 = 0;
    var l: u8 = 0;

    while (try bufferReader.readUntilDelimiterOrEof(buffer[0..], '\n')) |chunk| : (i += 1) {
        var iter = std.mem.splitAny(u8, chunk, " ");
        while (iter.next()) |element| {
            if (i == 0) {
                try stdout.print("Dimension: {d}\n", .{DIMENSION});

                distanceMatrix = try allocateMatrix(DIMENSION);
                flowMatrix = try allocateMatrix(DIMENSION);
            }
            if (i > 1 and i < DIMENSION + 2) {
                // Distance matrix
                try addNextToMatrix(&k, &l, true, element);
            }
            if (i > DIMENSION + 2) {
                // Flow matrix
                try addNextToMatrix(&k, &l, false, element);
            }
        }
    }

    try printMatrix(distanceMatrix);
    try printMatrix(flowMatrix);
}

fn generateOrder(order: []usize) !void {
    var prng = std.Random.DefaultPrng.init(blk: {
        var seed: u64 = undefined;
        try std.posix.getrandom(std.mem.asBytes(&seed));
        break :blk seed;
    });
    for (0..order.len) |i| {
        order[i] = i;
        stdoutMutex.lock();
        std.debug.print("{}\n", .{i});
        stdoutMutex.unlock();
    }

    std.Random.shuffle(prng.random(), usize, order);
}

fn generateParent(parent: *Instance) !void {
    try generateOrder(&parent.order);
    parent.calculateCost();

    stdoutMutex.lock();
    try stdout.print("{any}\t{}\n", .{ parent.order, parent.cost });
    stdoutMutex.unlock();
    var i: usize = 0;
    while (i < DEPTH) : (i += 1) {
        try populate(parent);
    }
}

fn evaluateParent(parents: []const Instance) usize {
    var bestCost: usize = std.math.maxInt(usize);
    var bestInstance: usize = undefined;
    for (0..parents.len) |i| {
        if (parents[i].cost < bestCost) {
            bestCost = parents[i].cost;
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
    var pivotPoints: [DIMENSION / 4]usize = undefined;
    for (&pivotPoints) |*point| {
        var randomPoint: usize = rand.uintLessThan(usize, DIMENSION);
        while (valueInArray(randomPoint, &pivotPoints)) {
            randomPoint = rand.uintLessThan(usize, DIMENSION);
        }
        point.* = randomPoint;
    }

    pivotSwap(parent, &pivotPoints, &child.order);
    child.calculateCost();

    stdoutMutex.lock();
    try stdout.print("{any} \t{}\n", .{ child.order, child.cost });
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
        if (children[i].cost < parent.cost) {
            for (0..parent.order.len) |j| {
                parent.order[j] = children[i].order[j];
            }
            parent.calculateCost();
        }
    }
}

pub fn main() !void {
    try readData(FILENAME);
    defer arenaMatrix.deinit();

    var threads: [PARENTS]Thread = undefined;
    var parentsTable: [PARENTS]Instance = undefined;
    for (0..threads.len) |i| {
        threads[i] = try Thread.spawn(.{}, generateParent, .{&parentsTable[i]});
    }

    for (0..threads.len) |i| {
        threads[i].join();
    }
    const bestInstance: usize = evaluateParent(&parentsTable);
    try stdout.print("\n\nThe best found cost is {} where order is: \n", .{parentsTable[bestInstance].cost});
    for (parentsTable[bestInstance].order) |elem| {
        try stdout.print("{} ", .{elem});
    }
    try stdout.print("\n", .{});
}

/// This imports the separate module containing `root.zig`. Take a look in `build.zig` for details.
const lib = @import("qap_lib");
