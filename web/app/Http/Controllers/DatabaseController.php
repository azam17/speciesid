<?php

namespace App\Http\Controllers;

use App\Models\ReferenceDatabase;
use Illuminate\Http\Request;
use Illuminate\Support\Facades\Gate;

class DatabaseController extends Controller
{
    public function index()
    {
        $databases = ReferenceDatabase::orderBy('created_at', 'desc')->get();

        return view('databases.index', compact('databases'));
    }

    public function show(ReferenceDatabase $database)
    {
        $database->load('calibrationProfiles');

        return view('databases.show', compact('database'));
    }

    public function create()
    {
        Gate::authorize('create', ReferenceDatabase::class);

        return view('databases.create');
    }

    public function store(Request $request)
    {
        Gate::authorize('create', ReferenceDatabase::class);
        $validated = $request->validate([
            'name' => 'required|string|max:255',
            'version' => 'required|string|max:64',
            'db_file' => 'required|file|max:512000',
            'index_file' => 'required|file|max:512000',
            'notes' => 'nullable|string',
        ]);

        $dbPath = $request->file('db_file')->store('databases', 'indexes');
        $idxPath = $request->file('index_file')->store('indexes', 'indexes');

        ReferenceDatabase::create([
            'name' => $validated['name'],
            'version' => $validated['version'],
            'file_path' => $dbPath,
            'index_path' => $idxPath,
            'sha256_hash' => hash_file('sha256', $request->file('db_file')->getRealPath()),
            'marker_panel' => ['COI', 'cytb', '12S', '16S'],
            'species_list' => [],
            'n_species' => 0,
            'n_markers' => 4,
            'notes' => $validated['notes'] ?? null,
        ]);

        return redirect()->route('databases.index')
            ->with('success', 'Database uploaded. Please run "speciesid index" to complete setup.');
    }

    public function activate(ReferenceDatabase $database)
    {
        Gate::authorize('activate', $database);
        ReferenceDatabase::where('is_active', true)->update(['is_active' => false]);
        $database->update(['is_active' => true]);

        return back()->with('success', "Database {$database->name} v{$database->version} activated.");
    }
}
