<?php

namespace App\Http\Controllers;

use App\Models\CalibrationProfile;
use App\Models\ReferenceDatabase;
use Illuminate\Http\Request;
use Illuminate\Support\Facades\Gate;

class CalibrationController extends Controller
{
    public function index()
    {
        $calibrations = CalibrationProfile::with('referenceDatabase')
            ->orderBy('created_at', 'desc')
            ->get();

        return view('calibrations.index', compact('calibrations'));
    }

    public function show(CalibrationProfile $calibration)
    {
        $calibration->load('referenceDatabase');

        return view('calibrations.show', compact('calibration'));
    }

    public function create()
    {
        Gate::authorize('create', CalibrationProfile::class);
        $databases = ReferenceDatabase::all();

        return view('calibrations.create', compact('databases'));
    }

    public function store(Request $request)
    {
        Gate::authorize('create', CalibrationProfile::class);
        $validated = $request->validate([
            'name' => 'required|string|max:255',
            'version' => 'required|string|max:64',
            'reference_database_id' => 'required|exists:reference_databases,id',
            'calibration_file' => 'required|file|max:10240',
            'notes' => 'nullable|string',
        ]);

        $path = $request->file('calibration_file')->store('calibrations', 'calibrations');

        CalibrationProfile::create([
            'name' => $validated['name'],
            'version' => $validated['version'],
            'file_path' => $path,
            'sha256_hash' => hash_file('sha256', $request->file('calibration_file')->getRealPath()),
            'reference_database_id' => $validated['reference_database_id'],
            'd_mu' => 0,
            'd_sigma' => 0.5,
            'b_mu' => 0,
            'b_sigma' => 0.5,
            'n_calibration_samples' => 0,
            'notes' => $validated['notes'] ?? null,
        ]);

        return redirect()->route('calibrations.index')
            ->with('success', 'Calibration profile uploaded.');
    }

    public function activate(CalibrationProfile $calibration)
    {
        Gate::authorize('activate', $calibration);
        CalibrationProfile::where('is_active', true)->update(['is_active' => false]);
        $calibration->update(['is_active' => true]);

        return back()->with('success', "Calibration {$calibration->name} v{$calibration->version} activated.");
    }
}
