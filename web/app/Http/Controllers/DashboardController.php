<?php

namespace App\Http\Controllers;

use App\Models\CalibrationProfile;
use App\Models\ReferenceDatabase;
use App\Models\Run;
use App\Models\Sample;

class DashboardController extends Controller
{
    public function index()
    {
        $user = request()->user();

        $recentRuns = Run::with('user')
            ->when(! $user->isLabManager(), fn ($q) => $q->where('user_id', $user->id))
            ->latest()
            ->limit(10)
            ->get();

        $pendingReview = Sample::whereIn('screening_result', ['ALERT', 'REVIEW'])
            ->whereHas('run', fn ($q) => $q->where('status', 'completed')
                ->when(! $user->isLabManager(), fn ($runQ) => $runQ->where('user_id', $user->id)))
            ->with('run')
            ->latest()
            ->limit(20)
            ->get();

        $failedControls = Sample::where('role', '!=', 'sample')
            ->where('screening_result', 'CONTROL_CONTAMINATION')
            ->whereHas('run', fn ($q) => $q->where('status', 'completed')
                ->when(! $user->isLabManager(), fn ($runQ) => $runQ->where('user_id', $user->id)))
            ->with('run')
            ->latest()
            ->limit(10)
            ->get();

        $stats = [
            'total_runs' => Run::query()
                ->when(! $user->isLabManager(), fn ($q) => $q->where('user_id', $user->id))
                ->count(),
            'runs_this_week' => Run::query()
                ->when(! $user->isLabManager(), fn ($q) => $q->where('user_id', $user->id))
                ->where('created_at', '>=', now()->subWeek())
                ->count(),
            'pending_review' => $pendingReview->count(),
            'failed_controls' => $failedControls->count(),
            'active_database' => ReferenceDatabase::where('is_active', true)->value('version') ?? 'N/A',
            'active_calibration' => CalibrationProfile::where('is_active', true)->value('version') ?? 'N/A',
        ];

        return view('dashboard', compact('recentRuns', 'pendingReview', 'failedControls', 'stats'));
    }
}
