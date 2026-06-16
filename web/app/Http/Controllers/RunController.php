<?php

namespace App\Http\Controllers;

use App\Models\Run;
use Illuminate\Http\Request;
use Illuminate\Support\Facades\Gate;

class RunController extends Controller
{
    public function index(Request $request)
    {
        $runs = Run::with('user')
            ->when(! $request->user()->isLabManager(), fn ($q) => $q->where('user_id', $request->user()->id))
            ->when($request->status, fn ($q, $s) => $q->where('status', $s))
            ->latest()
            ->paginate(25);

        return view('runs.index', compact('runs'));
    }

    public function show(Run $run)
    {
        Gate::authorize('view', $run);
        $run->load('user', 'referenceDatabase', 'calibrationProfile', 'samples', 'reports');

        return view('runs.show', compact('run'));
    }

    public function qc(Run $run)
    {
        Gate::authorize('view', $run);
        $run->load('samples');

        return view('runs.qc', compact('run'));
    }

    public function destroy(Run $run)
    {
        Gate::authorize('delete', $run);
        $run->delete();

        return redirect()->route('runs.index')
            ->with('success', 'Run deleted.');
    }
}
